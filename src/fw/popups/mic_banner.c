/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "popups/mic_banner.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_circle.h"
#include "applib/graphics/text.h"
#include "applib/ui/animation_interpolate.h"
#include "applib/ui/property_animation.h"
#include "applib/ui/window.h"
#include "applib/ui/window_stack.h"
#include "applib/unobstructed_area_service.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/util/math.h"
#include "popups/timeline/peek.h"
#include "process_management/app_manager.h"

#define FRAME_VISIBLE GRect(0, DISP_ROWS - MIC_BANNER_HEIGHT, DISP_COLS, MIC_BANNER_HEIGHT)
#define FRAME_HIDDEN  GRect(0, DISP_ROWS, DISP_COLS, MIC_BANNER_HEIGHT)
#define DOT_RADIUS    (4)
#define DOT_MARGIN    (8)

typedef struct {
  Window window;
  Layer strip;          //!< The visible bar; its frame is what slides in and out
  Animation *animation; //!< Currently running slide, if any
  bool visible;         //!< Target state: shown (or showing) vs hidden (or hiding)
} MicBanner;

static MicBanner s_banner;

// The window is transparent; only the strip draws.
static void prv_window_update_proc(Layer *layer, GContext *ctx) {
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer->bounds;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &bounds);

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  const char *text = i18n_get("Listening", &s_banner);
  const int16_t text_height = 18;
  const int16_t max_text_width = bounds.size.w - (3 * DOT_MARGIN) - (2 * DOT_RADIUS);
#if PBL_ROUND
  // The strip sits in a chord: centre the content horizontally, and vertically on the visible
  // segment's centroid rather than the strip's middle, which is mostly off screen.
  const GSize text_size = graphics_text_layout_get_max_used_size(
      ctx, text, font, GRect(0, 0, max_text_width, text_height), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft, NULL);
  const int16_t group_width = (2 * DOT_RADIUS) + DOT_MARGIN + text_size.w;
  const int16_t x0 = (bounds.size.w - group_width) / 2;
  const int16_t centre_y = (bounds.size.h * 2) / 5;
#else
  const int16_t x0 = DOT_MARGIN;
  const int16_t centre_y = bounds.size.h / 2;
#endif

  const GPoint dot = GPoint(x0 + DOT_RADIUS, centre_y);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  graphics_fill_circle(ctx, dot, DOT_RADIUS);

  // Gothic 14 renders its caps in the top ~9 rows of the box; this puts them on the dot's centre.
  const GRect text_box = GRect(dot.x + DOT_RADIUS + DOT_MARGIN, centre_y - (text_height / 2),
                               max_text_width, text_height);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text, font, text_box, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

// Obstruction
////////////////////////////////////////////////////////////////////////////////

//! Framebuffer-space y where a strip whose top is at display row `strip_y` starts obstructing the
//! app, or the framebuffer height if it doesn't overlap.
static int16_t prv_obstruction_for_strip_y(int16_t strip_y) {
  GSize fb_size;
  app_manager_get_framebuffer_size(&fb_size);
  if (fb_size.h != DISP_ROWS) {
    // Legacy-sized apps predate the microphone API; don't report an obstruction to them.
    return fb_size.h;
  }
  return MIN(strip_y, DISP_ROWS);
}

//! The app sees the union of the banner and Timeline Peek.
static int16_t prv_compose(int16_t banner_y) {
  return MIN(banner_y, timeline_peek_get_obstruction_origin_y());
}

int16_t mic_banner_get_obstruction_origin_y(void) {
  return prv_obstruction_for_strip_y(s_banner.strip.frame.origin.y);
}

// Slide animation (mirrors Timeline Peek)
////////////////////////////////////////////////////////////////////////////////

static void prv_frame_setup(Animation *animation) {
  PropertyAnimation *prop_anim = (PropertyAnimation *)animation;
  GRect from, to;
  property_animation_get_from_grect(prop_anim, &from);
  property_animation_get_to_grect(prop_anim, &to);
  unobstructed_area_service_will_change(prv_compose(prv_obstruction_for_strip_y(from.origin.y)),
                                        prv_compose(prv_obstruction_for_strip_y(to.origin.y)));
}

static void prv_frame_update(Animation *animation, AnimationProgress progress) {
  PropertyAnimation *prop_anim = (PropertyAnimation *)animation;
  property_animation_update_grect(prop_anim, progress);
  GRect to;
  property_animation_get_to_grect(prop_anim, &to);
  unobstructed_area_service_change(prv_compose(mic_banner_get_obstruction_origin_y()),
                                   prv_compose(prv_obstruction_for_strip_y(to.origin.y)), progress);
}

static void prv_frame_teardown(Animation *animation) {
  PropertyAnimation *prop_anim = (PropertyAnimation *)animation;
  GRect to;
  property_animation_get_to_grect(prop_anim, &to);
  unobstructed_area_service_did_change(prv_compose(prv_obstruction_for_strip_y(to.origin.y)));
}

static GRect prv_frame_getter(void *subject) {
  MicBanner *banner = subject;
  GRect frame;
  layer_get_frame(&banner->strip, &frame);
  return frame;
}

static void prv_frame_setter(void *subject, GRect frame) {
  MicBanner *banner = subject;
  layer_set_frame(&banner->strip, &frame);
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

static void prv_slide_stopped(Animation *animation, bool finished, void *context) {
  MicBanner *banner = &s_banner;
  banner->animation = NULL;
  if (finished && !banner->visible) {
    window_stack_remove(&banner->window, false /* animated */);
    i18n_free_all(banner);
  }
}

static const AnimationHandlers s_slide_handlers = {
  .stopped = prv_slide_stopped,
};

static void prv_slide_to(MicBanner *banner, GRect *to_frame) {
  if (banner->animation) {
    animation_unschedule(banner->animation);
    banner->animation = NULL;
  }
  PropertyAnimation *prop_anim = property_animation_create(&s_slide_impl, banner, NULL, NULL);
  property_animation_set_from_grect(prop_anim, &banner->strip.frame);
  property_animation_set_to_grect(prop_anim, to_frame);
  Animation *animation = property_animation_get_animation(prop_anim);
  animation_set_duration(animation, interpolate_moook_duration());
  animation_set_custom_interpolation(animation, interpolate_moook);
  animation_set_handlers(animation, s_slide_handlers, NULL);
  banner->animation = animation;
  animation_schedule(animation);
}

// Public API
////////////////////////////////////////////////////////////////////////////////

void mic_banner_init(void) {
  MicBanner *banner = &s_banner;
  *banner = (MicBanner){};
  window_init(&banner->window, WINDOW_NAME("Mic Banner"));
  window_set_focusable(&banner->window, false);
  window_set_transparent(&banner->window, true);
  layer_set_update_proc(&banner->window.layer, prv_window_update_proc);
  GRect hidden = FRAME_HIDDEN;
  layer_init(&banner->strip, &hidden);
  layer_set_update_proc(&banner->strip, prv_update_proc);
  layer_add_child(&banner->window.layer, &banner->strip);
}

void mic_banner_show(void) {
  MicBanner *banner = &s_banner;
  if (banner->visible) {
    return;
  }
  banner->visible = true;
  if (!banner->animation) {
    // Fresh show: start off screen, then slide up. A show during a hide just reverses.
    GRect hidden = FRAME_HIDDEN;
    layer_set_frame(&banner->strip, &hidden);
    modal_window_push(&banner->window, ModalPriorityDiscreet, false /* animated */);
  }
  GRect visible = FRAME_VISIBLE;
  prv_slide_to(banner, &visible);
}

void mic_banner_hide(void) {
  MicBanner *banner = &s_banner;
  if (!banner->visible) {
    return;
  }
  banner->visible = false;
  GRect hidden = FRAME_HIDDEN;
  prv_slide_to(banner, &hidden);
}

bool mic_banner_is_visible(void) {
  return s_banner.visible;
}
