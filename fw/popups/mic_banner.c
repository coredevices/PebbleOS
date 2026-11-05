/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/i18n/i18n.h>
#include <pbl/services/mic_manager.h>
#include <pbl/util/math.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/gdraw_command_image.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/graphics_circle.h>
#include <applib/graphics/text.h>
#include <applib/ui/animation_interpolate.h>
#include <applib/ui/property_animation.h>
#include <applib/ui/window.h>
#include <applib/ui/window_stack.h>
#include <applib/unobstructed_area_service.h>
#include <kernel/ui/modals/modal_manager.h>
#include <popups/mic_banner.h>
#include <popups/system_obstruction.h>
#include <process_management/app_manager.h>
#include <resource/resource_ids.auto.h>
#include <shell/prefs.h>

#define ICON_MIN_TOP (4)

#if PBL_RECT
#define LARGE_DISPLAY (DISP_COLS >= 200)
//! Black border on the top edge, and on the right edge once shrunk, as Timeline Peek's. Only in
//! black and white, where the white bar needs it to stand out.
#define BORDER            PBL_IF_COLOR_ELSE(0, 2)
#define MIC_BANNER_HEIGHT (LARGE_DISPLAY ? 38 : 30)
#define ICON_MARGIN       (LARGE_DISPLAY ? 12 : 8)
#define ICON_SIZE         (25)
//! The corner square the bar shrinks into: as tall as the bar, including its top and right border
#define SQUARE_SIZE (MIC_BANNER_HEIGHT)
//! Rubber band shrink: wind up to the right, snap past the square, bounce back, settle
#define COLLAPSE_DURATION_MS (400)
#define WIND_UP_PX           (8)
#define OVERSHOOT_PX         (6)
#define BOUNCE_PX            (2)
//! How long the full bar stays before shrinking into the corner
#define COLLAPSE_DELAY_MS (3000)
#define FONT_KEY          (LARGE_DISPLAY ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD)
#endif

//! Fill below the shape, so it still meets the bottom edge while the slide in overshoots upwards
#define SHAPE_HEIGHT (MIC_BANNER_HEIGHT + INTERPOLATE_MOOOK_BOUNCE_BACK)

typedef struct {
  Window window;
  Layer shape; //!< The visible dome or bar; its frame is what slides in and out
  GDrawCommandImage *icon;
  Animation *animation; //!< Currently running slide or shrink, if any
  bool visible;         //!< Target state: shown (or showing) vs hidden (or hiding)
} MicBanner;

static MicBanner s_banner;

#if PBL_ROUND
//! Width of the chord where the dome meets the bottom edge
static int16_t prv_shape_width(void) {
  const int32_t r = MIC_BANNER_RADIUS;
  const int32_t d = MIC_BANNER_RADIUS - MIC_BANNER_HEIGHT;
  return 2 * integer_sqrt((r * r) - (d * d));
}
#else
static int16_t prv_shape_width(void) {
  return DISP_COLS;
}
#endif

static GRect prv_frame(bool visible) {
  const int16_t w = prv_shape_width();
  return GRect((DISP_COLS - w) / 2, visible ? DISP_ROWS - MIC_BANNER_HEIGHT : DISP_ROWS, w,
               SHAPE_HEIGHT);
}

// The window is transparent; only the shape draws.
static void prv_window_update_proc(Layer *layer, GContext *ctx) {
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer->bounds;
#if PBL_ROUND
  const GSize icon = s_banner.icon ? gdraw_command_image_get_bounds_size(s_banner.icon) : GSizeZero;
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_circle(ctx, GPoint(bounds.size.w / 2, MIC_BANNER_RADIUS), MIC_BANNER_RADIUS);
  const GPoint icon_origin =
      GPoint((bounds.size.w - icon.w) / 2, MAX((MIC_BANNER_HEIGHT - icon.h) / 2, ICON_MIN_TOP));
#else
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, &bounds);

  // Shrinking from the bar into the square, 0 to 1; the mic moves to the square's centre
  const int32_t progress =
      CLIP(((DISP_COLS - bounds.size.w) * ANIMATION_NORMALIZED_MAX) / (DISP_COLS - SQUARE_SIZE), 0,
           ANIMATION_NORMALIZED_MAX);
  const int16_t square_margin = (SQUARE_SIZE - BORDER - ICON_SIZE) / 2;
  const GPoint icon_origin =
      GPoint(ICON_MARGIN + (((square_margin - ICON_MARGIN) * progress) / ANIMATION_NORMALIZED_MAX),
             BORDER + square_margin);

  // The text slides left with the bar's right edge and disappears behind the mic
  const int16_t gap = ICON_MARGIN / 2;
  const int16_t text_start = ICON_MARGIN + ICON_SIZE + gap;
  const int16_t line_h = LARGE_DISPLAY ? 34 : 28;
  const int16_t text_top =
      BORDER + ((MIC_BANNER_HEIGHT - BORDER - line_h) / 2) - (LARGE_DISPLAY ? 4 : 3);
  const GRect text_box =
      GRect(text_start - (DISP_COLS - bounds.size.w), text_top, DISP_COLS, line_h);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n_get("Listening...", &s_banner), fonts_get_system_font(FONT_KEY),
                     text_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, nullptr);
  const GRect icon_box = GRect(0, BORDER, icon_origin.x + ICON_SIZE + gap, bounds.size.h);
  graphics_fill_rect(ctx, &icon_box);

#if !PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &GRect(0, 0, bounds.size.w, BORDER));
  if (bounds.size.w < DISP_COLS) {
    graphics_fill_rect(ctx, &GRect(bounds.size.w - BORDER, 0, BORDER, bounds.size.h));
  }
#endif
#endif

  if (s_banner.icon) {
    gdraw_command_image_draw(ctx, s_banner.icon, icon_origin);
  }
}

static void prv_release(MicBanner *banner) {
  window_stack_remove(&banner->window, false /* animated */);
  gdraw_command_image_destroy(banner->icon);
  banner->icon = nullptr;
  i18n_free_all(banner);
}

// Obstruction
////////////////////////////////////////////////////////////////////////////////

//! Whether a smaller app is scaled up to the display, as Timeline Peek checks.
static bool prv_is_upscaled_app(const GSize *fb_size) {
  if (fb_size->h == DISP_ROWS) {
    return false;
  }
#if defined(CONFIG_APP_SCALING) && !defined(CONFIG_RECOVERY_FW)
  return shell_prefs_get_legacy_app_render_mode() >= LegacyAppRenderMode_ScalingNearest;
#else
  return false;
#endif
}

//! Framebuffer-space y where a shape whose top is at display row `shape_y` starts obstructing the
//! app, or the framebuffer height if it doesn't overlap. Scaled like Timeline Peek, as an app
//! built for a smaller platform can use the mic too.
static int16_t prv_obstruction_for_shape_y(int16_t shape_y) {
  GSize fb_size;
  app_manager_get_framebuffer_size(&fb_size);
  int16_t y = MIN(shape_y, DISP_ROWS);
  if (prv_is_upscaled_app(&fb_size)) {
    y = (y * fb_size.h) / DISP_ROWS;
  }
  return MIN(y, fb_size.h);
}

static int16_t prv_compose(int16_t banner_y) {
  return system_obstruction_compose(SystemOverlayMicBanner, banner_y);
}

int16_t mic_banner_get_obstruction_origin_y(void) {
  return prv_obstruction_for_shape_y(s_banner.shape.frame.origin.y);
}

#if PBL_RECT
//! Whether an app holds the mic, so the banner is showing or about to
static bool prv_app_uses_mic(void) {
#ifdef CONFIG_SERVICE_MIC_MANAGER
  const MicClient owner = mic_manager_get_owner();
  return (owner != MicClientNone) && (owner != MicClientVoiceDictation);
#else
  return false;
#endif
}
#endif

bool mic_banner_get_accessory_area(GRect *area_out) {
  *area_out = GRectZero;
#if PBL_RECT
  GSize fb_size;
  app_manager_get_framebuffer_size(&fb_size);
  if (prv_obstruction_for_shape_y(prv_frame(true).origin.y) >= fb_size.h) {
    return false; // Never overlaps the app
  }
  // Only the top moves. Read it once: apps call this from their own task.
  int16_t top = prv_obstruction_for_shape_y(s_banner.shape.frame.origin.y);
  if (top >= fb_size.h) {
    // Off screen. Once the app has the mic it is about to slide in, so report it just below the
    // screen: the app never sees it missing while it uses the mic.
    if (!s_banner.visible && !prv_app_uses_mic()) {
      return false;
    }
    top = fb_size.h;
  }
  GRect area = GRect(SQUARE_SIZE, top, DISP_COLS - SQUARE_SIZE, MIC_BANNER_HEIGHT);
  if (prv_is_upscaled_app(&fb_size)) {
    area.origin.x = (SQUARE_SIZE * fb_size.w) / DISP_COLS;
    area.size.w = fb_size.w - area.origin.x;
    area.size.h = (MIC_BANNER_HEIGHT * fb_size.h) / DISP_ROWS;
  }
  *area_out = area;
  return true;
#else
  return false;
#endif
}

// Slide animation (mirrors Timeline Peek)
////////////////////////////////////////////////////////////////////////////////

static void prv_frame_setup(Animation *animation) {
  PropertyAnimation *prop_anim = (PropertyAnimation *)animation;
  GRect from, to;
  property_animation_get_from_grect(prop_anim, &from);
  property_animation_get_to_grect(prop_anim, &to);
  unobstructed_area_service_will_change(prv_compose(prv_obstruction_for_shape_y(from.origin.y)),
                                        prv_compose(prv_obstruction_for_shape_y(to.origin.y)));
}

static void prv_frame_update(Animation *animation, AnimationProgress progress) {
  PropertyAnimation *prop_anim = (PropertyAnimation *)animation;
  property_animation_update_grect(prop_anim, progress);
  GRect to;
  property_animation_get_to_grect(prop_anim, &to);
  unobstructed_area_service_change(prv_compose(mic_banner_get_obstruction_origin_y()),
                                   prv_compose(prv_obstruction_for_shape_y(to.origin.y)), progress);
}

static void prv_frame_teardown(Animation *animation) {
  PropertyAnimation *prop_anim = (PropertyAnimation *)animation;
  GRect to;
  property_animation_get_to_grect(prop_anim, &to);
  unobstructed_area_service_did_change(prv_compose(prv_obstruction_for_shape_y(to.origin.y)));
}

static GRect prv_frame_getter(void *subject) {
  MicBanner *banner = subject;
  GRect frame;
  layer_get_frame(&banner->shape, &frame);
  return frame;
}

static void prv_frame_setter(void *subject, GRect frame) {
  MicBanner *banner = subject;
  layer_set_frame(&banner->shape, &frame);
}

static const PropertyAnimationImplementation s_slide_impl = {
  .base =
      {
        .setup = prv_frame_setup,
        .update = prv_frame_update,
        .teardown = prv_frame_teardown,
      },
  .accessors = {
    .getter.grect = prv_frame_getter,
    .setter.grect = prv_frame_setter,
  },
};

#if PBL_RECT
static void prv_schedule_collapse(MicBanner *banner);
#endif

static void prv_slide_stopped(Animation *animation, bool finished, void *context) {
  MicBanner *banner = &s_banner;
  banner->animation = nullptr;
  if (!finished) {
    return;
  }
  if (!banner->visible) {
    prv_release(banner);
  }
#if PBL_RECT
  else if (banner->shape.frame.size.w == DISP_COLS) {
    prv_schedule_collapse(banner);
  }
#endif
}

static const AnimationHandlers s_slide_handlers = {
  .stopped = prv_slide_stopped,
};

static void prv_slide_to(MicBanner *banner, GRect *to_frame) {
  if (banner->animation) {
    animation_unschedule(banner->animation);
    banner->animation = nullptr;
  }
  PropertyAnimation *prop_anim = property_animation_create(&s_slide_impl, banner, nullptr, nullptr);
  property_animation_set_from_grect(prop_anim, &banner->shape.frame);
  property_animation_set_to_grect(prop_anim, to_frame);
  Animation *animation = property_animation_get_animation(prop_anim);
  animation_set_duration(animation, interpolate_moook_duration());
  animation_set_custom_interpolation(animation, interpolate_moook);
  animation_set_handlers(animation, s_slide_handlers, nullptr);
  banner->animation = animation;
  animation_schedule(animation);
}

#if PBL_RECT
// Shrink into the corner square
////////////////////////////////////////////////////////////////////////////////

//! Eases between two widths over [start, end] of the animation's progress, quadratically in
//! (accelerating) or out (decelerating).
static int16_t prv_segment(AnimationProgress progress, int32_t start, int32_t end, int16_t from,
                           int16_t to, bool ease_in) {
  const int64_t max = ANIMATION_NORMALIZED_MAX;
  const int64_t t = CLIP(((int64_t)(progress - start) * max) / (end - start), 0, max);
  const int64_t eased = ease_in ? (t * t) / max : (t * ((2 * max) - t)) / max;
  return from + (int16_t)(((to - from) * eased) / max);
}

//! Only the width changes, so the obstruction stays the same. Widths past the screen edge push
//! the text right, which is what shows the wind up.
static void prv_collapse_update(Animation *animation, AnimationProgress progress) {
  const int32_t max = ANIMATION_NORMALIZED_MAX;
  const int32_t wind_up_end = (max * 30) / 100;
  const int32_t snap_end = (max * 55) / 100;
  const int32_t bounce_end = (max * 78) / 100;
  int16_t w;
  if (progress < wind_up_end) {
    w = prv_segment(progress, 0, wind_up_end, DISP_COLS, DISP_COLS + WIND_UP_PX, false);
  } else if (progress < snap_end) {
    w = prv_segment(progress, wind_up_end, snap_end, DISP_COLS + WIND_UP_PX,
                    SQUARE_SIZE - OVERSHOOT_PX, true);
  } else if (progress < bounce_end) {
    w = prv_segment(progress, snap_end, bounce_end, SQUARE_SIZE - OVERSHOOT_PX,
                    SQUARE_SIZE + BOUNCE_PX, false);
  } else {
    w = prv_segment(progress, bounce_end, max, SQUARE_SIZE + BOUNCE_PX, SQUARE_SIZE, false);
  }
  GRect frame = s_banner.shape.frame;
  frame.size.w = w;
  layer_set_frame(&s_banner.shape, &frame);
}

static const AnimationImplementation s_collapse_impl = {
  .update = prv_collapse_update,
};

static void prv_collapse_stopped(Animation *animation, bool finished, void *context) {
  s_banner.animation = nullptr;
}

static void prv_schedule_collapse(MicBanner *banner) {
  Animation *animation = animation_create();
  animation_set_implementation(animation, &s_collapse_impl);
  animation_set_delay(animation, COLLAPSE_DELAY_MS);
  animation_set_duration(animation, COLLAPSE_DURATION_MS);
  animation_set_curve(animation, AnimationCurveLinear);
  animation_set_handlers(animation, (AnimationHandlers){.stopped = prv_collapse_stopped}, nullptr);
  banner->animation = animation;
  animation_schedule(animation);
}
#endif

// Public API
////////////////////////////////////////////////////////////////////////////////

void mic_banner_init(void) {
  MicBanner *banner = &s_banner;
  *banner = (MicBanner){};
  window_init(&banner->window, WINDOW_NAME("Mic Banner"));
  window_set_focusable(&banner->window, false);
  window_set_transparent(&banner->window, true);
  layer_set_update_proc(&banner->window.layer, prv_window_update_proc);
  GRect hidden = prv_frame(false);
  layer_init(&banner->shape, &hidden);
  layer_set_update_proc(&banner->shape, prv_update_proc);
  layer_add_child(&banner->window.layer, &banner->shape);
}

void mic_banner_show(void) {
  MicBanner *banner = &s_banner;
  if (banner->visible) {
    return;
  }
  banner->visible = true;
  if (!banner->animation) {
    // Fresh show: start off screen, then slide up. A show during a hide just reverses.
    GRect hidden = prv_frame(false);
    layer_set_frame(&banner->shape, &hidden);
    if (!banner->icon) {
      banner->icon =
          gdraw_command_image_create_with_resource_system(SYSTEM_APP, RESOURCE_ID_MIC_BANNER_ICON);
    }
    modal_window_push(&banner->window, ModalPriorityDiscreet, false /* animated */);
  }
  GRect visible = banner->shape.frame;
  visible.origin.y = prv_frame(true).origin.y;
  prv_slide_to(banner, &visible);
}

void mic_banner_hide(void) {
  MicBanner *banner = &s_banner;
  if (!banner->visible) {
    return;
  }
  banner->visible = false;
  GRect hidden = banner->shape.frame;
  hidden.origin.y = DISP_ROWS;
  prv_slide_to(banner, &hidden);
}

void mic_banner_handle_process_kill(void) {
  MicBanner *banner = &s_banner;
  const bool on_screen = banner->visible || banner->animation;
  banner->visible = false;
  if (banner->animation) {
    animation_unschedule(banner->animation);
    banner->animation = nullptr;
  }
  if (!on_screen) {
    return;
  }
  GRect hidden = prv_frame(false);
  layer_set_frame(&banner->shape, &hidden);
  prv_release(banner);
}

bool mic_banner_is_visible(void) {
  return s_banner.visible;
}
