/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_fetch_ui.h"

#include <inttypes.h>
#include <string.h>

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/app_fetch_endpoint.h>
#include <pbl/services/battery/battery_charger.h>
#include <pbl/services/compositor/compositor_transitions.h>
#include <pbl/services/compositor/default/compositor_dot_transitions.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/trig.h>

#include <applib/app.h>
#include <applib/app_focus_service.h>
#include <applib/app_malloc.h>
#include <applib/app_timer.h>
#include <applib/fonts/fonts.h>
#include <applib/graphics/bitblt.h>
#include <applib/graphics/gbitmap_png.h>
#include <applib/graphics/gdraw_command_frame.h>
#include <applib/graphics/gdraw_command_image.h>
#include <applib/graphics/gdraw_command_sequence.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/progress_window.h>
#include <applib/ui/ui.h>
#include <process_management/app_install_manager.h>
#include <process_management/worker_manager.h>
#include <process_state/app_state/app_state.h>
#include <resource/resource_ids.auto.h>
#include <shell/normal/watchface.h>
#include <shell/system_theme.h>
#include <syscall/syscall.h>

#define FAIL_PAUSE_MS         1000
#define SCROLL_OUT_MS         250
#define BAR_HEIGHT            6
#define BAR_WIDTH             80
#define BAR_TO_TRANS_MS       160
#define TRANS_TO_DOT_MS       90
#define DOT_TRANSITION_RADIUS 13
#define DOT_COMPOSITOR_RADIUS 7
#define DOT_OFFSET            25
#define UPDATE_INTERVAL       200
#define UPDATE_AMOUNT         2
#define FAILURE_PERCENT       15
#define INITIAL_PERCENT       0

#if APP_FETCH_UI_GIFT_SUPPORTED
#define GIFT_TICK_MS 50
//! Frames in a half turn, after which the box looks the same. A plain half turn is followed by
//! one in which the box is hit from inside.
#define GIFT_HALF_TURN_FRAMES 32
#define GIFT_LOOP_FRAMES      (2 * GIFT_HALF_TURN_FRAMES)
//! After the loop, the box facing the viewer in two parts for the reveal: its lid and its body
#define GIFT_LID_FRAME  GIFT_LOOP_FRAMES
#define GIFT_BODY_FRAME (GIFT_LOOP_FRAMES + 1)
#define GIFT_FRAMES     (GIFT_LOOP_FRAMES + 2)
//! Frame of the plain half turn that faces the viewer
#define GIFT_FACING_FRAME  (GIFT_HALF_TURN_FRAMES / 2)
#define GIFT_CIRCLE_RADIUS 65
//! Covers the circle's anti-aliased edge and the corners of the largest icon as it closes
#define GIFT_IRIS_MARGIN 3
//! The most the fill rises per tick while the app arrives, and once it has, in px
#define GIFT_FILL_STEP        1
#define GIFT_FILL_FINISH_STEP 48
//! Ticks of the eased turn to face the viewer, and of the hold after it
#define GIFT_FACE_TURN_TICKS 8
#define GIFT_HOLD_TICKS      1
//! Ticks the icon is shown alone, and over which the circle then closes to the launch's dot
#define GIFT_ICON_SHOW_TICKS 6
#define GIFT_CLOSE_TICKS     3
#define GIFT_DOT_RADIUS      (DOT_ANIMATION_STROKE_WIDTH / 2)
//! How far below the circle's centre the box's near rim is, in px
#define GIFT_RIM_DY 2
//! Scale the icon is drawn at. A larger or heavier icon is replaced by the rocket.
#define GIFT_ICON_SCALE     3
#define GIFT_ICON_MAX_SIDE  32
#define GIFT_ICON_MAX_BYTES (8 * 1024)

//! Reveal keyframes, one per tick, in px: how far the lid and the body have moved, and how far
//! below the circle's centre the icon is. The last entry holds.
static const int16_t s_gift_lid_y[] = {0, 2, -14, -54, -130, -200};
static const int16_t s_gift_body_y[] = {0, -2, 10, 46, 120, 230};
static const int16_t s_gift_icon_y[] = {60, 56, 32, 6, -5, 1, 0};
//! The reveal ends with its keyframes: the lid and the body have left the screen by then
#define GIFT_REVEAL_TICKS (PBL_ARRAY_SIZE(s_gift_icon_y) - 1)

typedef enum {
  //! Spinning, and hit from inside, while the app arrives
  GiftPhaseSpin,
  //! The app has arrived: the fill rises to the top as the box spins on
  GiftPhaseFill,
  //! An eased turn to face the viewer, then a hold
  GiftPhaseTurn,
  GiftPhaseHold,
  //! The lid flies off, the body drops away and the icon rises out of it, then rests
  GiftPhaseReveal,
  GiftPhaseShow,
  //! The circle closes to the dot the app launches from
  GiftPhaseClose,
} GiftPhase;
#endif

//! App data
typedef struct {
  //! UI
  ProgressWindow window;

  //! App fetch result
  AppFetchResult result;

  //! Data
  AppInstallEntry install_entry;
  AppFetchUIArgs next_app_args;
  EventServiceInfo fetch_event_info;
  EventServiceInfo connect_event_info;

  bool done;

#if APP_FETCH_UI_GIFT_SUPPORTED
  //! Gift box scene, drawn over the progress window
  Layer gift_layer;
  GDrawCommandSequence *gift_frames;
  uint8_t gift_frame;
  //! What the box reveals: the app's icon, or a rocket
  GBitmap *gift_icon;
  GDrawCommandImage *gift_rocket;
  AppTimer *gift_timer;
  GiftPhase gift_phase;
  uint8_t gift_loop_frame;
  uint8_t gift_phase_tick;
  //! The eased turn: its first frame and how many frames it covers
  uint8_t gift_turn_from;
  uint8_t gift_turn_span;
  int16_t gift_fill_height;
  bool gift_shown;
  //! The app has arrived and the scene runs on to the launch
  bool gift_finishing;
  //! The watch buzzed as the gift opened
  bool gift_buzzed;
#endif
} AppFetchUIData;

static void prv_set_progress(AppFetchUIData *data, int16_t progress) {
  progress_window_set_progress(&data->window, progress);
}

//! @return whether a finished fetch buzzes the watch
static bool prv_should_buzz(const AppFetchUIData *data) {
  // if this was launched by the phone, it's probably a new install
  return (data->next_app_args.common.reason == APP_LAUNCH_PHONE) && !battery_is_usb_connected();
}

//! @return whether the gift box scene already buzzed the watch as it opened
static bool prv_gift_buzzed(const AppFetchUIData *data) {
#if APP_FETCH_UI_GIFT_SUPPORTED
  return data->gift_buzzed;
#else
  return false;
#endif
}

// Launch the desired app
static void prv_app_fetch_launch_app(AppFetchUIData *data) {
  // Let's launch the application we just fetched.
  PBL_LOG_DBG("App Fetch: Putting launch event");

  if (prv_should_buzz(data) && !prv_gift_buzzed(data)) {
    vibes_short_pulse();
  }

  // Allocate and initialize the data that would have been sent to the app originally before the
  // fetch request.
  PebbleLaunchAppEventExtended *ext = pbl_malloc_check(sizeof(PebbleLaunchAppEventExtended));
  *ext = (PebbleLaunchAppEventExtended){
    .common = data->next_app_args.common,
    .wakeup = data->next_app_args.wakeup_info
  };
  ext->common.transition = compositor_dot_transition_app_fetch_get();
  if ((data->next_app_args.common.reason == APP_LAUNCH_WAKEUP) &&
      (data->next_app_args.common.args != nullptr)) {
    ext->common.args = &data->next_app_args.wakeup_info;
  }

  PebbleEvent launch_event = {
    .type = PEBBLE_APP_LAUNCH_EVENT,
    .launch_app = {.id = data->next_app_args.app_id, .data = ext}
  };

  event_put(&launch_event);
}

//! Launches the fetched app, unless it is gone
static void prv_launch_fetched_app(AppFetchUIData *data) {
  AppInstallEntry entry;
  if (!app_install_get_entry_for_install_id(data->next_app_args.app_id, &entry)) {
    PBL_LOG_WRN("App Fetch: entry %" PRId32 " gone after fetch, not launching",
                data->next_app_args.app_id);
    app_window_stack_pop(true);
    return;
  }

  prv_app_fetch_launch_app(data);
}

///////////////////////////////
// Gift Box Scene
///////////////////////////////

#if APP_FETCH_UI_GIFT_SUPPORTED
static int16_t prv_gift_keyframe(const int16_t *keys, size_t count, uint8_t tick) {
  return keys[PBL_MIN(tick, count - 1)];
}

static GSize prv_gift_icon_size(const AppFetchUIData *data) {
  if (data->gift_icon) {
    return gbitmap_get_bounds(data->gift_icon).size;
  }
  if (data->gift_rocket) {
    return gdraw_command_image_get_bounds_size(data->gift_rocket);
  }
  return GSizeZero;
}

//! Draws the icon centred on `center`, clipped below `rim` (layer coordinates)
static void prv_gift_draw_icon(const AppFetchUIData *data, GContext *ctx, GPoint center,
                               int16_t rim) {
  const GSize size = prv_gift_icon_size(data);
  const GPoint origin = GPoint(center.x - (size.w / 2), center.y - (size.h / 2));
  const GRect saved_clip_box = ctx->draw_state.clip_box;
  const GRect above_rim = {
    .origin = ctx->draw_state.drawing_box.origin,
    .size = GSize(data->gift_layer.bounds.size.w, PBL_MAX(0, rim)),
  };
  ctx->draw_state.clip_box = above_rim;
  grect_clip(&ctx->draw_state.clip_box, &saved_clip_box);
  if (data->gift_icon) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, data->gift_icon, &(GRect){origin, size});
  } else if (data->gift_rocket) {
    gdraw_command_image_draw(ctx, data->gift_rocket, origin);
  }
  ctx->draw_state.clip_box = saved_clip_box;
}

//! Draws a frame of the sequence centred `dy` below `center`
static void prv_gift_draw_frame(const AppFetchUIData *data, GContext *ctx, uint8_t index,
                                GPoint center, int16_t dy) {
  const GSize size = gdraw_command_sequence_get_bounds_size(data->gift_frames);
  const GPoint origin = GPoint(center.x - (size.w / 2), center.y - (size.h / 2) + dy);
  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(data->gift_frames, index);
  gdraw_command_frame_draw(ctx, data->gift_frames, frame, origin);
}

static void prv_gift_layer_update_proc(Layer *layer, GContext *ctx) {
  const AppFetchUIData *data = app_state_get_user_data();
  const GRect *bounds = &layer->bounds;
  const GPoint center = grect_center_point(bounds);
  const GColor fill_color = APP_FETCH_UI_FILL_COLOR;
  const int16_t unfilled_height = bounds->size.h - data->gift_fill_height;

  graphics_context_set_fill_color(ctx, system_theme_get_bg_color());
  graphics_fill_rect(ctx, &GRect(0, 0, bounds->size.w, unfilled_height));
  graphics_context_set_fill_color(ctx, fill_color);
  graphics_fill_rect(ctx, &GRect(0, unfilled_height, bounds->size.w, data->gift_fill_height));

  const uint8_t close_tick = (data->gift_phase == GiftPhaseClose) ? data->gift_phase_tick : 0;
  if (close_tick >= GIFT_CLOSE_TICKS) {
    // The dot the launch transition opens from, drawn as it draws it
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, DOT_ANIMATION_STROKE_WIDTH);
    graphics_draw_line(ctx, center, center);
    return;
  }

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, GIFT_CIRCLE_RADIUS);

  switch (data->gift_phase) {
    case GiftPhaseReveal: {
      const uint8_t tick = data->gift_phase_tick;
      const int16_t lid_y = prv_gift_keyframe(s_gift_lid_y, PBL_ARRAY_SIZE(s_gift_lid_y), tick);
      const int16_t body_y = prv_gift_keyframe(s_gift_body_y, PBL_ARRAY_SIZE(s_gift_body_y), tick);
      const int16_t icon_y = prv_gift_keyframe(s_gift_icon_y, PBL_ARRAY_SIZE(s_gift_icon_y), tick);
      // The icon is inside the box until it has risen past the rim
      const int16_t rim = center.y + GIFT_RIM_DY + body_y;
      prv_gift_draw_icon(data, ctx, GPoint(center.x, center.y + icon_y), rim);
      prv_gift_draw_frame(data, ctx, GIFT_BODY_FRAME, center, body_y);
      prv_gift_draw_frame(data, ctx, GIFT_LID_FRAME, center, lid_y);
      break;
    }
    case GiftPhaseShow:
    case GiftPhaseClose:
      prv_gift_draw_icon(data, ctx, center, bounds->size.h);
      break;
    default:
      prv_gift_draw_frame(data, ctx, data->gift_frame, center, 0);
      break;
  }

  if (close_tick > 0) {
    // A ring of the fill covers the circle outside its closing radius
    const int16_t radius =
        GIFT_CIRCLE_RADIUS -
        (((GIFT_CIRCLE_RADIUS - GIFT_DOT_RADIUS) * close_tick) / GIFT_CLOSE_TICKS);
    const int16_t outer_radius = GIFT_CIRCLE_RADIUS + GIFT_IRIS_MARGIN;
    const int16_t diameter = (2 * outer_radius) + 1;
    graphics_context_set_fill_color(ctx, fill_color);
    graphics_fill_radial(
        ctx, GRect(center.x - outer_radius, center.y - outer_radius, diameter, diameter),
        GOvalScaleModeFitCircle, outer_radius - radius, 0, TRIG_MAX_ANGLE);
  }
}

static void prv_gift_show_frame(AppFetchUIData *data, uint8_t frame) {
  data->gift_frame = frame;
}

static void prv_gift_show_next_frame(AppFetchUIData *data) {
  data->gift_loop_frame = (data->gift_loop_frame + 1) % GIFT_LOOP_FRAMES;
  prv_gift_show_frame(data, data->gift_loop_frame);
}

static void prv_gift_set_phase(AppFetchUIData *data, GiftPhase phase) {
  data->gift_phase = phase;
  data->gift_phase_tick = 0;
}

//! Counts a tick of the current phase.
//! @return false once the phase has had `ticks` of them
static bool prv_gift_phase_tick(AppFetchUIData *data, uint8_t ticks) {
  if (data->gift_phase_tick >= ticks) {
    return false;
  }
  data->gift_phase_tick++;
  return true;
}

static void prv_gift_begin_turn(AppFetchUIData *data) {
  // A half turn, and on to the next frame that faces the viewer
  data->gift_turn_from = (data->gift_loop_frame + 1) % GIFT_HALF_TURN_FRAMES;
  data->gift_turn_span =
      GIFT_HALF_TURN_FRAMES +
      ((GIFT_FACING_FRAME + GIFT_HALF_TURN_FRAMES - data->gift_turn_from) % GIFT_HALF_TURN_FRAMES);
  prv_gift_set_phase(data, GiftPhaseTurn);
}

static void prv_gift_show_turn_frame(AppFetchUIData *data) {
  const int32_t t = data->gift_phase_tick;
  const int32_t ticks = GIFT_FACE_TURN_TICKS;
  // Smoothstep, t^2 (3 - 2t)
  const int32_t eased =
      (data->gift_turn_span * t * t * ((3 * ticks) - (2 * t))) / (ticks * ticks * ticks);
  prv_gift_show_frame(data, (data->gift_turn_from + eased) % GIFT_HALF_TURN_FRAMES);
}

static void prv_gift_begin_reveal(AppFetchUIData *data) {
  prv_gift_set_phase(data, GiftPhaseReveal);
  // Two buzzes as the gift opens stand in for the one at the launch
  if (prv_should_buzz(data)) {
    vibes_double_pulse();
    data->gift_buzzed = true;
  }
}

static void prv_gift_timer_callback(void *context) {
  AppFetchUIData *data = context;
  // One-shot timers, so that a late tick is not followed by a burst of them
  data->gift_timer = app_timer_register(GIFT_TICK_MS, prv_gift_timer_callback, data);

  const int16_t height = data->gift_layer.bounds.size.h;
  switch (data->gift_phase) {
    case GiftPhaseSpin: {
      prv_gift_show_next_frame(data);
      const int16_t target = (height * data->window.progress_percent) / MAX_PROGRESS_PERCENT;
      data->gift_fill_height =
          PBL_MAX(data->gift_fill_height, PBL_MIN(target, data->gift_fill_height + GIFT_FILL_STEP));
      break;
    }
    case GiftPhaseFill:
      if (data->gift_fill_height < height) {
        prv_gift_show_next_frame(data);
        data->gift_fill_height = PBL_MIN(height, data->gift_fill_height + GIFT_FILL_FINISH_STEP);
        break;
      }
      prv_gift_begin_turn(data);
      [[fallthrough]];
    case GiftPhaseTurn:
      if (prv_gift_phase_tick(data, GIFT_FACE_TURN_TICKS)) {
        prv_gift_show_turn_frame(data);
        break;
      }
      prv_gift_set_phase(data, GiftPhaseHold);
      [[fallthrough]];
    case GiftPhaseHold:
      if (prv_gift_phase_tick(data, GIFT_HOLD_TICKS)) {
        // Nothing has moved
        return;
      }
      prv_gift_begin_reveal(data);
      break;
    case GiftPhaseReveal:
      if (!prv_gift_phase_tick(data, GIFT_REVEAL_TICKS)) {
        prv_gift_set_phase(data, GiftPhaseShow);
      }
      break;
    case GiftPhaseShow:
      if (prv_gift_phase_tick(data, GIFT_ICON_SHOW_TICKS)) {
        return;
      }
      prv_gift_set_phase(data, GiftPhaseClose);
      [[fallthrough]];
    case GiftPhaseClose:
      if (!prv_gift_phase_tick(data, GIFT_CLOSE_TICKS)) {
        // The dot has been up for a tick. The scene stays up for the launch transition.
        app_timer_cancel(data->gift_timer);
        data->gift_timer = nullptr;
        app_focus_service_unsubscribe();
        data->gift_finishing = false;
        prv_launch_fetched_app(data);
        return;
      }
      break;
  }
  layer_mark_dirty(&data->gift_layer);
}

static void prv_gift_will_focus(bool in_focus) {
  AppFetchUIData *data = app_state_get_user_data();
  // No ticks for a screen that is covered, unless they lead to the launch
  if (!in_focus && !data->gift_finishing && data->gift_timer) {
    app_timer_cancel(data->gift_timer);
    data->gift_timer = nullptr;
  }
}

static void prv_gift_did_focus(bool in_focus) {
  AppFetchUIData *data = app_state_get_user_data();
  if (in_focus && !data->gift_timer) {
    data->gift_timer = app_timer_register(GIFT_TICK_MS, prv_gift_timer_callback, data);
  }
}

static void prv_gift_start(AppFetchUIData *data) {
  data->gift_frames = gdraw_command_sequence_create_with_resource(RESOURCE_ID_APP_FETCH_GIFT);
  if (!data->gift_frames) {
    return;
  }
  if (gdraw_command_sequence_get_num_frames(data->gift_frames) < GIFT_FRAMES) {
    gdraw_command_sequence_destroy(data->gift_frames);
    data->gift_frames = nullptr;
    return;
  }

  Layer *root_layer = window_get_root_layer(&data->window.window);
  layer_init(&data->gift_layer, &root_layer->bounds);
  layer_set_update_proc(&data->gift_layer, prv_gift_layer_update_proc);
  layer_add_child(root_layer, &data->gift_layer);
  // The covered bar would redraw the window each time it moves
  layer_remove_from_parent(&data->window.progress_layer.layer);
  data->gift_shown = true;

  // The box starts to spin once the screen is in view
  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .will_focus = prv_gift_will_focus,
    .did_focus = prv_gift_did_focus,
  });
}

//! Removes the scene, leaving the plain progress window. Does nothing if it is not shown.
//! @param launch whether to launch the fetched app if the scene was on its way to that
static void prv_gift_stop(AppFetchUIData *data, bool launch) {
  if (!data->gift_shown) {
    return;
  }
  data->gift_shown = false;
  app_focus_service_unsubscribe();
  if (data->gift_timer) {
    app_timer_cancel(data->gift_timer);
    data->gift_timer = nullptr;
  }
  layer_remove_from_parent(&data->gift_layer);
  layer_add_child(window_get_root_layer(&data->window.window), &data->window.progress_layer.layer);
  gdraw_command_sequence_destroy(data->gift_frames);
  data->gift_frames = nullptr;
  gbitmap_destroy(data->gift_icon);
  data->gift_icon = nullptr;
  gdraw_command_image_destroy(data->gift_rocket);
  data->gift_rocket = nullptr;

  const bool finishing = data->gift_finishing;
  data->gift_finishing = false;
  if (finishing && launch) {
    prv_launch_fetched_app(data);
  }
}

//! @return whether the icon is a PNG larger than the scene shows. The decoder allocates what
//! the header claims before it reads a pixel.
static bool prv_gift_icon_png_is_too_large(ResAppNum bank, uint32_t icon_id) {
  // The signature, then the IHDR chunk: its length, its type, the width and the height
  uint32_t header[6];
  if ((sys_resource_load_range(bank, icon_id, 0, (uint8_t *)header, sizeof(header)) !=
       sizeof(header)) ||
      !gbitmap_png_data_is_png((const uint8_t *)header, sizeof(header))) {
    return false;
  }
  return (pbl_be32_to_cpu(header[4]) > GIFT_ICON_MAX_SIDE) ||
         (pbl_be32_to_cpu(header[5]) > GIFT_ICON_MAX_SIDE);
}

//! @return the app's menu icon scaled up, or nullptr if it has none or it cannot be used
static GBitmap *prv_gift_create_app_icon(const AppInstallEntry *entry) {
  const uint32_t icon_id = app_install_entry_get_icon_resource_id(entry);
  if (icon_id == RESOURCE_ID_INVALID) {
    return nullptr;
  }
  // Like the launcher, skip a third-party icon whose resource is too large to load
  const ResAppNum bank = app_install_get_app_icon_bank(entry);
  if ((bank != SYSTEM_APP) && ((sys_resource_size(bank, icon_id) > GIFT_ICON_MAX_BYTES) ||
                               prv_gift_icon_png_is_too_large(bank, icon_id))) {
    return nullptr;
  }
  GBitmap *icon = gbitmap_create_with_resource_system(bank, icon_id);
  if (!icon) {
    return nullptr;
  }
  const GSize size = gbitmap_get_bounds(icon).size;
  if ((size.w <= 0) || (size.h <= 0) || (size.w > GIFT_ICON_MAX_SIDE) ||
      (size.h > GIFT_ICON_MAX_SIDE)) {
    gbitmap_destroy(icon);
    return nullptr;
  }

  // Copy to 8-bit first, whatever its format, keeping transparency. A 1-bit icon is black ink.
  GBitmap *plain = gbitmap_create_blank(size, GBitmapFormat8Bit);
  if (plain) {
    const bool one_bit = (gbitmap_get_format(icon) == GBitmapFormat1Bit);
    bitblt_bitmap_into_bitmap(plain, icon, GPointZero, one_bit ? GCompOpTint : GCompOpAssign,
                              GColorBlack);
  }
  gbitmap_destroy(icon);

  const GSize scaled_size = GSize(size.w * GIFT_ICON_SCALE, size.h * GIFT_ICON_SCALE);
  GBitmap *scaled = plain ? gbitmap_create_blank(scaled_size, GBitmapFormat8Bit) : nullptr;
  if (scaled) {
    const uint8_t *src = gbitmap_get_data(plain);
    const uint16_t src_stride = gbitmap_get_bytes_per_row(plain);
    uint8_t *dst = gbitmap_get_data(scaled);
    const uint16_t dst_stride = gbitmap_get_bytes_per_row(scaled);
    for (int16_t y = 0; y < scaled_size.h; y++) {
      for (int16_t x = 0; x < scaled_size.w; x++) {
        dst[(y * dst_stride) + x] =
            src[((y / GIFT_ICON_SCALE) * src_stride) + (x / GIFT_ICON_SCALE)];
      }
    }
  }
  gbitmap_destroy(plain);
  return scaled;
}

//! Starts the ending: the fill reaches the top, the box turns to face the viewer and opens on
//! the app's icon, the circle closes and the app launches.
//! @return false if the scene is not shown
static bool prv_gift_finish(AppFetchUIData *data) {
  if (!data->gift_shown) {
    return false;
  }
  data->gift_finishing = true;
  // The icon arrived with the app's resources
  data->gift_icon = prv_gift_create_app_icon(&data->install_entry);
  if (!data->gift_icon) {
    data->gift_rocket = gdraw_command_image_create_with_resource(RESOURCE_ID_APP_FETCH_GIFT_ROCKET);
  }
  prv_gift_set_phase(data, GiftPhaseFill);
  // The ending runs on to the launch even while the screen is covered
  if (!data->gift_timer) {
    data->gift_timer = app_timer_register(GIFT_TICK_MS, prv_gift_timer_callback, data);
  }
  return true;
}
#else
static void prv_gift_start(AppFetchUIData *data) {
}

static void prv_gift_stop(AppFetchUIData *data, bool launch) {
}

static bool prv_gift_finish(AppFetchUIData *data) {
  return false;
}
#endif

///////////////////////////////
// Animation Related Functions
///////////////////////////////

static void prv_remote_comm_session_event_handler(PebbleEvent *event, void *context) {
  AppFetchUIData *data = app_state_get_user_data();
  if (event->bluetooth.comm_session_event.is_open &&
      event->bluetooth.comm_session_event.is_system) {
    progress_window_pop(&data->window);
  }
}

static void prv_set_progress_failure(AppFetchUIData *data) {
  const bool launch = false;
  prv_gift_stop(data, launch);

  uint32_t icon;
  const char *message;
  switch (data->result) {
    case AppFetchResultNoBluetooth:
      icon = TIMELINE_RESOURCE_WATCH_DISCONNECTED;
      message = pbl_i18n_get("Not connected", data);
      // Subscribe to the BT remote app connect event
      data->connect_event_info = (EventServiceInfo){
        .type = PEBBLE_COMM_SESSION_EVENT,
        .handler = prv_remote_comm_session_event_handler
      };
      event_service_client_subscribe(&data->connect_event_info);
      break;
    case AppFetchResultNoData:
      icon = TIMELINE_RESOURCE_CHECK_INTERNET_CONNECTION;
#if PBL_ROUND
      // TODO: peek layer should do its own line wrapping
      message = pbl_i18n_get("No internet\nconnection", data);
#else
      message = pbl_i18n_get("No internet connection", data);
#endif
      break;
    case AppFetchResultIncompatibleJSFailure:
      icon = TIMELINE_RESOURCE_GENERIC_WARNING;
      message = pbl_i18n_get("Incompatible JS", data);
      break;
    case AppFetchResultGeneralFailure:
    case AppFetchResultUUIDInvalid:
    case AppFetchResultPutBytesFailure:
    case AppFetchResultTimeoutError:
    case AppFetchResultPhoneBusy:
    default:
      icon = TIMELINE_RESOURCE_GENERIC_WARNING;
      message = pbl_i18n_get("Failed", data);
      break;
  }

  progress_window_set_result_failure(&data->window, icon, message,
                                     PROGRESS_WINDOW_DEFAULT_FAILURE_DELAY_MS);

  if (!battery_is_usb_connected()) {
    vibes_short_pulse();
  }
}

static void prv_progress_window_finished(ProgressWindow *window, bool success, void *context) {
  if (success) {
    prv_launch_fetched_app(context);
  }
}

////////////////////////////
// Internal Helper Functions
////////////////////////////

//! Used to clean up the application's data before exiting
static void prv_app_fetch_cleanup(AppFetchUIData *data) {
  PBL_LOG_DBG("App Fetch: prv_app_fetch_cleanup");
  event_service_client_unsubscribe(&data->fetch_event_info);
  event_service_client_unsubscribe(&data->connect_event_info);
}

//! Used when the app fetch process has failed
static void prv_app_fetch_failure(AppFetchUIData *data, uint8_t error_code) {
  PBL_LOG_WRN("App Fetch: prv_app_fetch_failure: %d", error_code);

  if (error_code == AppFetchResultUserCancelled) {
    app_window_stack_pop(true);
  }
  data->result = error_code;

  if ((watchface_get_default_install_id() == data->install_entry.install_id) &&
      app_install_entry_is_watchface(&data->install_entry)) {
    // We failed to fetch a watchface and it was our default.
    // Invalidate it and it will be reassigned to one that exists next time around.
    PBL_LOG_WRN("Default watchface fetch failed, setting INVALID as default");
    watchface_set_default_install_id(INSTALL_ID_INVALID);
  } else if ((worker_manager_get_default_install_id() == data->install_entry.install_id) &&
             app_install_entry_has_worker(&data->install_entry)) {
    // We failed to fetch a worker and it was our default.
    // Invalidate it and it will be reassigned to one that is launched next.
    PBL_LOG_WRN("Default worker fetch failed, setting INVALID as default");
    worker_manager_set_default_install_id(INSTALL_ID_INVALID);
  }

  data->done = true;
  prv_set_progress_failure(data);
  prv_app_fetch_cleanup(data);
}

//! App Fetch handler. Used for keeping track of progress and cleanup events
static void prv_app_fetch_event_handler(PebbleEvent *event, void *context) {
  AppFetchUIData *data = app_state_get_user_data();
  PebbleAppFetchEvent *af_event = (PebbleAppFetchEvent *)event;

  // We have starting the App Fetch Process
  if (af_event->type == AppFetchEventTypeStart) {
    PBL_LOG_DBG("App Fetch: Got the start event");

    // We have received a new progress event
  } else if (af_event->type == AppFetchEventTypeProgress) {
    progress_window_set_progress(&data->window, af_event->progress_percent);

    // We have finished the app fetch. Launching
  } else if (af_event->type == AppFetchEventTypeFinish) {
    data->done = true;
    if (!prv_gift_finish(data)) {
      progress_window_set_result_success(&data->window);
    }
    prv_app_fetch_cleanup(data);

    // We received an error. Fail
  } else if (af_event->type == AppFetchEventTypeError) {
    prv_app_fetch_failure(data, af_event->error_code);
  }
}

// TODO: Use appropriate transitions to and from watchfaces or apps
static void prv_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppFetchUIData *data = app_state_get_user_data();
  if (data->done) {
    // A button skips the rest of the gift box scene
    const bool launch = true;
    prv_gift_stop(data, launch);
    app_window_stack_pop(true);
  } else {
    app_fetch_cancel(data->install_entry.install_id);
  }
}

static void config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_click_handler);
}

static void handle_init(void) {
  AppFetchUIData *data = app_zalloc_check(sizeof(AppFetchUIData));
  app_state_set_user_data(data);

  // get app args, copy them to app memory, and free the kernel buffer
  AppFetchUIArgs *temp_fetch_args = (AppFetchUIArgs *)process_manager_get_current_process_args();
  memcpy(&data->next_app_args, temp_fetch_args, sizeof(AppFetchUIArgs));
  pbl_free(temp_fetch_args);

  // Create and set up window
  progress_window_init(&data->window);
  progress_window_set_callbacks(&data->window,
                                (ProgressWindowCallbacks){
                                  .finished = prv_progress_window_finished,
                                },
                                data);
  window_set_click_config_provider((Window *)&data->window, config_provider);

  // retrieve data about the AppInstallId given
  if (!app_install_get_entry_for_install_id(data->next_app_args.app_id, &data->install_entry)) {
    PBL_LOG_ERR("App Fetch: Error getting entry for id: %" PRIu32 "", data->next_app_args.app_id);
    return;
  }

  AppFetchError prev_error = app_fetch_get_previous_error();
  if ((prev_error.id == data->next_app_args.app_id) &&
      (prev_error.error != AppFetchResultSuccess)) {
    prv_app_fetch_failure(data, prev_error.error);
    prv_set_progress(data, FAILURE_PERCENT);
  }

  // subscribe to PutBytes events
  data->fetch_event_info =
      (EventServiceInfo){.type = PEBBLE_APP_FETCH_EVENT, .handler = prv_app_fetch_event_handler};
  event_service_client_subscribe(&data->fetch_event_info);

  if (!data->done) {
    prv_gift_start(data);
  }
  app_progress_window_push(&data->window);
}

static void handle_deinit(void) {
  AppFetchUIData *data = app_state_get_user_data();
  prv_app_fetch_cleanup(data);
  const bool launch = false;
  prv_gift_stop(data, launch);
  progress_window_deinit(&data->window);
  app_free(data);
  pbl_i18n_free_all(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd *app_fetch_ui_get_app_info() {
  static const PebbleProcessMdSystem s_app_md = {
    .common =
        {
          .main_func = s_main,
          .visibility = ProcessVisibilityHidden,
          // UUID: 674271bc-f4fa-4536-97f3-8849a5ba75a4
          .uuid =
              {0x67, 0x42, 0x71, 0xbc, 0xf4, 0xfa, 0x45, 0x36, 0x97, 0xf3, 0x88, 0x49, 0xa5, 0xba,
               0x75, 0xa4},
        },
    .name = "App Fetch",
  };
  return (const PebbleProcessMd *)&s_app_md;
}
