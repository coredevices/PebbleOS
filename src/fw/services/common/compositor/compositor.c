/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "compositor.h"
#include "compositor_dma.h"
#include "compositor_display.h"

#include "applib/graphics/bitblt.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gcontext.h"
#include "applib/graphics/gtypes.h"
#include "applib/ui/animation.h"
#include "applib/ui/animation_private.h"
#include "drivers/display/display.h"
#include "kernel/event_loop.h"
#include "kernel/kernel_applib_state.h"
#include "kernel/ui/kernel_ui.h"
#include "kernel/ui/modals/modal_manager.h"
#include "mcu/cache.h"
#include "popups/timeline/peek.h"
#include "process_management/app_manager.h"
#include "process_management/process_manager.h"
#include "process_state/app_state/app_state.h"
#include "shell/prefs.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/profiler.h"
#include "util/size.h"
#include "util/math.h"

// The number of pixels for a given row which get set to black to round the corner. These numbers
// are for the top-left corner, but can easily be translated to the other corners. This is used by
// bezel mode to replicate the behavior of the FPGA.
#if PBL_COLOR
static const uint32_t s_rounded_corner_width[6] = { 6, 4, 3, 2, 1, 1 };
#endif

//! This is our root framebuffer that everything gets composited into.
static FrameBuffer DMA_BSS s_framebuffer;

typedef enum {
  //! Render the app with no transparent modals straight through
  CompositorState_App,
  //! Render the opaque modal straight through
  CompositorState_Modal,
  //! Render the app with transparent modals straight through
  CompositorState_AppAndModal,
  //!< Waiting for the app to render itself so we can start the transition
  CompositorState_AppTransitionPending,
  //!< Compositor is running a transition animation
  CompositorState_Transitioning,
} CompositorState;

//! Deferred render struct is used to handle a render event initiated while a display update is in
//! progress and the update is non-blocking on the platform (ie. snowy/bobby smiles).
typedef struct {
  struct {
    bool pending;
    AnimationProgress progress;
  } animation;
  struct {
    bool pending;
  } transition_complete;
  struct {
    bool pending;
  } app;
  struct {
    bool pending;
    const CompositorTransition *compositor_animation;
  } transition_start;
} DeferredRender;

typedef struct {
  Animation *animation;
  const CompositorTransition *impl;
  GPoint modal_offset;
} CompositorTransitionState;

static CompositorState s_state;

static DeferredRender s_deferred_render;

static CompositorTransitionState s_animation_state;

static bool s_framebuffer_frozen;

//! Animation .update function for the AnimationImplementation we use to drive our transitions.
//! Wraps the .update function of the current CompositorTransition.
static void prv_animation_update(Animation *animation, const AnimationProgress distance_normalized);

//! Call this function whenever a transition completes to change the state to one of the stable
//! states (CompositorState_App or CompositorState_Modal).
static void prv_finish_transition(void);

void compositor_init(void) {
#if CAPABILITY_COMPOSITOR_USES_DMA && !TARGET_QEMU && !UNITTEST
  compositor_dma_init();
#endif

  const GSize fb_size = GSize(DISP_COLS, DISP_ROWS);
  framebuffer_init(&s_framebuffer, &fb_size);
  framebuffer_clear(&s_framebuffer);

  s_state = CompositorState_App;

  s_deferred_render = (DeferredRender) {
    .animation.pending = false,
    .app.pending = false
  };

  s_animation_state = (CompositorTransitionState) { 0 };

  s_framebuffer_frozen = false;
}

// Helper functions to make implementing transitions easier
///////////////////////////////////////////////////////////

void compositor_app_framebuffer_fill_callback(GContext *ctx, int16_t y,
                                              Fixed_S16_3 x_range_begin, Fixed_S16_3 x_range_end,
                                              Fixed_S16_3 delta_begin, Fixed_S16_3 delta_end,
                                              void *user_data) {
  const GPoint *offset = user_data ?: &GPointZero; // User data has left the building
  compositor_scaled_app_fb_copy(
    GRect(
      x_range_begin.integer - offset->x, y - offset->y,
      x_range_end.integer - x_range_begin.integer, 1
    ),
    true /* copy_relative_to_origin */
  );
}

#if PBL_COLOR
static int prv_get_rounded_corner_width(int row_index, int num_rows) {
  if (row_index >= num_rows) {
    return 0;
  }
  if (row_index < (int)ARRAY_LENGTH(s_rounded_corner_width)) {
    return s_rounded_corner_width[row_index];
  } else if (num_rows - row_index - 1 < (int)ARRAY_LENGTH(s_rounded_corner_width)) {
    return s_rounded_corner_width[num_rows - row_index - 1];
  }
  return 0;
}
#endif

void compositor_set_modal_transition_offset(GPoint modal_offset) {
  s_animation_state.modal_offset = modal_offset;
}

void compositor_render_app(void) {
  PBL_ASSERT_TASK(PebbleTask_KernelMain);

  PROFILER_NODE_START(compositor);

  // Don't trust the size field within the app framebuffer as the app could modify it.
  GSize app_framebuffer_size;
  app_manager_get_framebuffer_size(&app_framebuffer_size);


#if CAPABILITY_COMPOSITOR_USES_DMA && !TARGET_QEMU && !UNITTEST
  if (gsize_equal(&app_framebuffer_size, &s_framebuffer.size)) {
    const FrameBuffer *app_framebuffer = app_state_get_framebuffer();
    compositor_dma_run(s_framebuffer.buffer, app_framebuffer->buffer, FRAMEBUFFER_SIZE_BYTES);
  } else {
#endif
    // Fill entire framebuffer with black first to avoid artifacts
    GBitmap dest_bitmap = compositor_get_framebuffer_as_bitmap();
    memset(dest_bitmap.addr, GColorBlack.argb, framebuffer_get_size_bytes(&s_framebuffer));

    compositor_scaled_app_fb_copy(GRect(0, 0, DISP_COLS, DISP_ROWS), false /* copy_relative_to_origin */);
#if CAPABILITY_COMPOSITOR_USES_DMA && !TARGET_QEMU && !UNITTEST
  }
#endif

  if (s_state == CompositorState_AppAndModal) {
    compositor_render_modal();
  }

  PROFILER_NODE_STOP(compositor);

  framebuffer_dirty_all(&s_framebuffer);
}

void compositor_render_modal(void) {
  GContext *ctx = kernel_ui_get_graphics_context();

  // We make this GDrawState static to save stack space, thus the declaration and init must be
  // performed on two separate lines because the initializer value is not constant
  static GDrawState prev_state;
  prev_state = ctx->draw_state;

  gpoint_add_eq(&ctx->draw_state.drawing_box.origin, s_animation_state.modal_offset);

  modal_manager_render(ctx);

  ctx->draw_state = prev_state;
}

// Compositor implementation
///////////////////////////////////////////////////////////

T_STATIC void prv_handle_display_update_complete(void) {
  if (s_deferred_render.transition_complete.pending) {
    s_deferred_render.transition_complete.pending = false;
    prv_finish_transition();
  }
  if (s_deferred_render.animation.pending) {
    s_deferred_render.animation.pending = false;
    prv_animation_update(s_animation_state.animation, s_deferred_render.animation.progress);
  }
  // Process transition_start before app so that the compositor state is set to
  // AppTransitionPending before compositor_app_render_ready() is called. Otherwise, the app
  // framebuffer may be rendered directly to the display before the transition animation starts.
  if (s_deferred_render.transition_start.pending) {
    s_deferred_render.transition_start.pending = false;
    compositor_transition(s_deferred_render.transition_start.compositor_animation);
  }
  if (s_deferred_render.app.pending) {
    s_deferred_render.app.pending = false;
    compositor_app_render_ready();
  }
}

static void prv_compositor_flush(void) {
  PBL_ASSERT_TASK(PebbleTask_KernelMain);

  // Stop the framebuffer_prepare performance timer. This timer was started when the client
  // first posted the render event to the system.
  compositor_display_update(prv_handle_display_update_complete);
}

static void prv_send_did_focus_event(bool in_focus) {
  PebbleEvent event = {
    .type = PEBBLE_APP_DID_CHANGE_FOCUS_EVENT,
    .app_focus = {
      .in_focus = in_focus,
    },
  };
  event_put(&event);
}

static bool prv_should_render(void) {
  return !(compositor_display_update_in_progress() || s_framebuffer_frozen);
}

static void prv_release_app_framebuffer(void) {
  // Inform the app that the render is complete and it is safe to write into its framebuffer again.
  PebbleEvent event = {
    .type = PEBBLE_RENDER_FINISHED_EVENT,
  };
  process_manager_send_event_to_process(PebbleTask_App, &event);
}

void compositor_app_render_ready(void) {
  if (!prv_should_render()) {
    s_deferred_render.app.pending = true;
    return;
  }

  if (s_state == CompositorState_AppTransitionPending) {
    // Huzzah, the app sent us the first frame!
    if (s_animation_state.animation) {
      // We have an animation to run, run it.
      s_state = CompositorState_Transitioning;
      animation_schedule(s_animation_state.animation);

      // Don't release the app framebuffer yet, we'll do this once the transition completes. This
      // way the app won't update its frame buffer while we're transitioning to it.
      return;
    } else {
      // No animation was used, immediately say that the app is now fully focused.
      const ModalProperty properties = modal_manager_get_properties();
      s_state = ((properties & ModalProperty_Exists) && (properties & ModalProperty_Transparent)) ?
          CompositorState_AppAndModal : CompositorState_App;
      prv_send_did_focus_event(true);
    }
  }

  // Draw the app framebuffer if in the App state
  if (s_state == CompositorState_App || s_state == CompositorState_AppAndModal) {
    // compositor_render_app also renders modals if the CompositorState_AppAndModal as that state
    // indicates that there are transparent modals that allow the app framebuffer to show through
    compositor_render_app();
    prv_compositor_flush();
  }

  // Draw the modal if in the Modal state
  if (s_state == CompositorState_Modal) {
    compositor_render_modal();
    prv_compositor_flush();
  }

  prv_release_app_framebuffer();
}

static void prv_send_app_render_request(void) {
  PebbleEvent event = {
    .type = PEBBLE_RENDER_REQUEST_EVENT,
  };
  process_manager_send_event_to_process(PebbleTask_App, &event);
}

void compositor_modal_render_ready(void) {
  if ((s_state == CompositorState_Transitioning) || !prv_should_render()) {
    // Don't let the modal redraw itself when the redraw loop is being currently driven by an
    // animation or if a display update is in progress.
    return;
  }

  if ((s_state == CompositorState_AppTransitionPending) &&
      (modal_manager_get_properties() & ModalProperty_Transparent)) {
    // Don't render if modals are transparent while the app is not ready yet
    return;
  }

  if (s_state == CompositorState_Modal) {
    compositor_render_modal();
    prv_compositor_flush();
  } else if (s_state == CompositorState_AppAndModal) {
    prv_send_app_render_request();
  }
}

void compositor_transition_render(CompositorTransitionUpdateFunc func, Animation *animation,
                                  const AnimationProgress distance_normalized) {
  if (!prv_should_render()) {
    if (!s_deferred_render.transition_complete.pending) {
      s_deferred_render.animation.pending = true;
      s_deferred_render.animation.progress = distance_normalized;
    }
    return;
  }
  GContext *ctx = kernel_ui_get_graphics_context();

  // Save the draw state in a static to save stack space
  static GDrawState prev_state;
  prev_state = ctx->draw_state;

  func(ctx, animation, distance_normalized);

  ctx->draw_state = prev_state;

  if (!s_animation_state.impl->skip_modal_render_after_update) {
    compositor_render_modal();
  }

  prv_compositor_flush();
}

static void prv_animation_update(Animation *animation,
                                 const AnimationProgress distance_normalized) {
  PBL_ASSERT_TASK(PebbleTask_KernelMain);
  // Since we might be running this animation update as part of a deferred render, we must
  // update the kernel animation state's .current_animation to point to this animation;
  // otherwise if the animation specified any custom spacial interpolation (e.g. moook), it would
  // be ignored
  AnimationPrivate *animation_private = animation_private_animation_find(animation);
  AnimationState *kernel_animation_state = kernel_applib_get_animation_state();
  PBL_ASSERTN(animation_private && kernel_animation_state && kernel_animation_state->aux);
  AnimationPrivate *saved_current_animation = kernel_animation_state->aux->current_animation;

  kernel_animation_state->aux->current_animation = animation_private;
  compositor_transition_render(s_animation_state.impl->update, animation, distance_normalized);
  kernel_animation_state->aux->current_animation = saved_current_animation;
}

static void prv_finish_transition(void) {
  const ModalProperty properties = modal_manager_get_properties();
  if (properties & ModalProperty_Exists) {
    s_state = (properties & ModalProperty_Transparent) ? CompositorState_AppAndModal :
                                                         CompositorState_Modal;
    compositor_modal_render_ready();

    // Force the app framebuffer to be released. We hold it during transitions to keep the app
    // framebuffer from changing while it's being animated but now that we're done we want to make
    // sure it's always available to the app. This is only needed when we're finishing to a modal
    // since compositor_app_render_ready will also release the framebuffer.
    prv_release_app_framebuffer();
  } else {
    s_state = CompositorState_App;
    compositor_app_render_ready();
  }

  prv_send_did_focus_event(properties & ModalProperty_Unfocused);
}

static void prv_animation_teardown(Animation *animation) {
  if (s_animation_state.impl->teardown) {
    s_animation_state.impl->teardown(animation);
  }
  s_animation_state = (CompositorTransitionState) { 0 };

  s_deferred_render.animation.pending = false;
  if (!prv_should_render()) {
    s_deferred_render.transition_complete.pending = true;
    return;
  }

  prv_finish_transition();
}

void compositor_transition(const CompositorTransition *compositor_animation) {
  if (s_animation_state.animation != NULL) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Animation <%u> in progress, cancelling",
            (int) s_animation_state.animation);

    animation_destroy(s_animation_state.animation);
    s_animation_state = (CompositorTransitionState) { 0 };

    s_deferred_render.animation.pending = false;
    s_deferred_render.transition_complete.pending = false;
  }

  if (!prv_should_render() || s_deferred_render.animation.pending) {
    if (s_deferred_render.app.pending) {
      s_deferred_render.app.pending = false;
      prv_release_app_framebuffer();
    }

    s_deferred_render.transition_start.pending = true;
    s_deferred_render.transition_start.compositor_animation = compositor_animation;
    return;
  }

  if (compositor_animation) {
    // Set up our animation state and schedule it

    s_animation_state = (CompositorTransitionState) {
      .animation = animation_create(),
      .impl = compositor_animation
    };

    static const AnimationImplementation s_compositor_animation_impl = {
      .update = prv_animation_update,
      .teardown = prv_animation_teardown,
    };
    animation_set_implementation(s_animation_state.animation, &s_compositor_animation_impl);

    compositor_animation->init(s_animation_state.animation);
  }

  const ModalProperty properties = modal_manager_get_properties();
  const bool is_modal_existing = (properties & ModalProperty_Exists);
  const bool is_modal_transparent = (properties & ModalProperty_Transparent);
  if (((s_state == CompositorState_Modal) && !is_modal_existing) || is_modal_transparent) {
    // Modal to App or Any to Transparent Modal

    // We can't say for sure whether or not the app framebuffer is in a reasonable state, as the
    // app could be redrawing itself right now. Since we can't query this, instead trigger the
    // app to redraw itself. This way we will cause an PEBBLE_RENDER_READY_EVENT in the very near
    // future, regardless of the app's state.
    prv_send_app_render_request();

    // Now wait for the ready event.
    s_state = CompositorState_AppTransitionPending;

  } else if (is_modal_existing  && !is_modal_transparent) {
    // Modal to Modal or App to Modal

    // We can start animating immediately if we're going to a modal window. This is because
    // modal window content is drawn on demand so it's always available.
    if (compositor_animation) {
      s_state = CompositorState_Transitioning;
      animation_schedule(s_animation_state.animation);
    } else {
      prv_finish_transition();
    }

  } else {
    // App to App

    // We have to wait for the app to populate its framebuffer
    s_state = CompositorState_AppTransitionPending;
  }
}

FrameBuffer *compositor_get_framebuffer(void) {
  return &s_framebuffer;
}

GBitmap compositor_get_framebuffer_as_bitmap(void) {
  return framebuffer_get_as_bitmap(&s_framebuffer, &s_framebuffer.size);
}

GBitmap compositor_get_app_framebuffer_as_bitmap(void) {
  // Get the app framebuffer state based on the size it should be to prevent a malicious app from
  // changing it and causing issues.
  GSize app_framebuffer_size;
  app_manager_get_framebuffer_size(&app_framebuffer_size);
  return framebuffer_get_as_bitmap(app_state_get_framebuffer(), &app_framebuffer_size);
}

bool compositor_is_animating(void) {
  return s_state == CompositorState_AppTransitionPending ||
         s_state == CompositorState_Transitioning;
}

void compositor_transition_cancel(void) {
  if (animation_is_scheduled(s_animation_state.animation)) {
    animation_unschedule(s_animation_state.animation);
  }
}

void compositor_freeze(void) {
  s_framebuffer_frozen = true;
}

static void prv_compositor_unfreeze_cb(void *ignored) {
  // Run deferred draws
  prv_handle_display_update_complete();
}

void compositor_unfreeze(void) {
  s_framebuffer_frozen = false;

  launcher_task_add_callback(prv_compositor_unfreeze_cb, NULL);
}

static bool prv_app_framebuffer_matches_display(void) {
  GSize app_framebuffer_size;
  app_manager_get_framebuffer_size(&app_framebuffer_size);
  return gsize_equal(&app_framebuffer_size, &s_framebuffer.size);
}

uint16_t prv_scale_coordinate(const uint32_t scale_factor, uint16_t val) {
  const uint32_t val_fixed = (uint32_t)val * scale_factor;
  return val_fixed >> 16;  // Get integer part
}

void compositor_scaled_app_fb_copy(const GRect update_rect, bool copy_relative_to_origin) {
  compositor_scaled_app_fb_copy_offset(update_rect, copy_relative_to_origin, 0 /* offset_y */);
}

void compositor_scaled_app_fb_copy_offset(const GRect update_rect, bool copy_relative_to_origin, int16_t offset_y) {
  GBitmap src_bitmap = compositor_get_app_framebuffer_as_bitmap();
  GBitmap dst_bitmap = compositor_get_framebuffer_as_bitmap();

  if (prv_app_framebuffer_matches_display()) {
    GBitmap sub_bitmap;
    gbitmap_init_as_sub_bitmap(&sub_bitmap, &src_bitmap, update_rect);
    bitblt_bitmap_into_bitmap(&dst_bitmap, &sub_bitmap, update_rect.origin, GCompOpAssign, GColorWhite);
    return;
  }

#if PBL_COLOR
  const int16_t app_width = src_bitmap.bounds.size.w;
  const int16_t app_height = src_bitmap.bounds.size.h;
  const int16_t disp_width = dst_bitmap.bounds.size.w;
  const int16_t disp_height = dst_bitmap.bounds.size.h;

#if CAPABILITY_HAS_APP_SCALING && !RECOVERY_FW
  // Check if we should use scaling mode for legacy apps
  if (shell_prefs_get_legacy_app_render_mode() == LegacyAppRenderMode_Scaling) {
    // Scale legacy apps to fill the display using nearest-neighbor scaling

    // Calculate scaling factors using fixed-point arithmetic (16.16 format)
    // This gives us sub-pixel precision for better scaling
    const uint32_t scale_x = ((uint32_t)app_width << 16) / disp_width;
    const uint32_t scale_y = ((uint32_t)app_height << 16) / disp_height;

    // Perform nearest-neighbor scaling
    for (int16_t dst_y = 0; dst_y < update_rect.size.h; dst_y++) {
      const int16_t dst_y_offset = dst_y + update_rect.origin.y + offset_y;
      const int16_t src_y = prv_scale_coordinate(scale_y, copy_relative_to_origin ? CLIP(dst_y_offset, 0, disp_height - 1) : dst_y);

      // Ensure we don't go out of bounds
      if (src_y < 0 || src_y >= app_height) continue;
      if (dst_y_offset < 0 || dst_y_offset >= disp_height) continue;

      GBitmapDataRowInfo dst_row_info = gbitmap_get_data_row_info(&dst_bitmap, dst_y_offset);
      GBitmapDataRowInfo src_row_info = gbitmap_get_data_row_info(&src_bitmap, src_y);
      uint8_t *dst_line = dst_row_info.data;
      uint8_t *src_line = src_row_info.data;

      for (int16_t dst_x = 0; dst_x < update_rect.size.w; dst_x++) {
        int16_t dst_x_offset = dst_x + update_rect.origin.x;
        const int16_t src_x = prv_scale_coordinate(scale_x, copy_relative_to_origin ? CLIP(dst_x_offset, 0, disp_width - 1) : dst_x);

        // Check if the source pixel is within valid range
        if (src_x < src_row_info.min_x || src_x > src_row_info.max_x) {
          // Source pixel is outside circular mask, leave destination black
          continue;
        }

        if (dst_x_offset < dst_row_info.min_x || dst_x_offset > dst_row_info.max_x) {
          continue;
        }

        // Copy the pixel from source to destination
        dst_line[dst_x_offset] = src_line[src_x];
      }
    }
  } else
#endif
  {
    // Original bezel mode - center with black bezel
    const int16_t bezel_width = (DISP_COLS - app_width) / 2;
    const int16_t bezel_height = (DISP_ROWS - app_height) / 2;
    const int16_t app_peek_offset_y = timeline_peek_get_origin_y() - app_height;
    const int16_t app_offset_y = CLIP(app_peek_offset_y, 0, bezel_height);
    PBL_ASSERTN((bezel_width > 0) && (bezel_height > 0));

    // memset the entire region to be updated to black
    int16_t first_row = CLIP(update_rect.origin.y, 0, DISP_ROWS - 1);
    int16_t last_row = CLIP(update_rect.origin.y + update_rect.size.h, first_row, DISP_ROWS);
    for (int16_t y = first_row; y < last_row; y++) {
      GBitmapDataRowInfo dst_row_info = gbitmap_get_data_row_info(&dst_bitmap, y);
      const int16_t start_x = MAX(update_rect.origin.x, dst_row_info.min_x);
      const int16_t end_x = MIN(update_rect.origin.x + update_rect.size.w, dst_row_info.max_x + 1);
      memset(&dst_row_info.data[start_x], GColorBlack.argb, end_x - start_x);
    }

    // bitblt the region of the app framebuffer into the display framebuffer
    GPoint dst_offset;
    GRect src_rect;

    if (copy_relative_to_origin) {
      GRect centered_region = GRect(bezel_width, app_offset_y, app_width, app_height);
      GRect clipped_update_region = update_rect;
      grect_clip(&clipped_update_region, &centered_region);

      src_rect = GRect(
        clipped_update_region.origin.x - bezel_width,
        clipped_update_region.origin.y - app_offset_y + offset_y,
        clipped_update_region.size.w,
        clipped_update_region.size.h
      );
      dst_offset = clipped_update_region.origin;
    } else {
      src_rect = GRect(
        0,
        offset_y,
        update_rect.size.w - bezel_width,
        update_rect.size.h - app_offset_y
      );
      dst_offset = GPoint(bezel_width - update_rect.origin.x, app_offset_y - update_rect.origin.y);
    }

    if (src_rect.size.w > 0 && src_rect.size.h > 0) {
      GBitmap sub_bitmap;
      gbitmap_init_as_sub_bitmap(&sub_bitmap, &src_bitmap, src_rect);
      bitblt_bitmap_into_bitmap(&dst_bitmap, &sub_bitmap, dst_offset, GCompOpAssign, GColorWhite);
    }
  }
#endif
}
