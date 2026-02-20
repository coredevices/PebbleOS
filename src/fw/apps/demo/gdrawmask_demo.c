/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gdrawmask_demo.h"

#include "applib/app.h"
#include "applib/app_light.h"
#include "process_state/app_state/app_state.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "syscall/syscall.h"
#include "util/size.h"
#include "util/trig.h"

typedef struct {
  Window window;
} GDrawMaskDemoData;

static void prv_draw_text(GContext *ctx, const GRect *layer_bounds) {
  graphics_context_set_text_color(ctx, GColorBlack);
  const char *text = PBL_IF_RECT_ELSE("Masks are fun!", "\nMasks are fun!");
  GTextAttributes *text_attributes = graphics_text_attributes_create();
  const uint8_t inset = 4;
  graphics_text_attributes_enable_screen_text_flow(text_attributes, inset);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD), *layer_bounds,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, text_attributes);
  graphics_text_attributes_destroy(text_attributes);
}

void prv_fill_mask_shape(GContext *ctx, const GRect *layer_bounds, const GSize *shape_size,
                         int32_t current_angle) {
  const GOvalScaleMode oval_scale_mode = GOvalScaleModeFitCircle;

  // Calculate the radial rect
  const GRect inset_layer_bounds = grect_inset((*layer_bounds), GEdgeInsets(shape_size->h / 2));
  const GRect shape_rect = grect_centered_from_polar(inset_layer_bounds, oval_scale_mode,
                                                     current_angle, (*shape_size));

  // Fill the radial circle
  graphics_fill_oval(ctx, shape_rect, oval_scale_mode);
}

static void prv_layer_update_proc(Layer *layer, GContext *ctx) {
  GDrawMaskDemoData *data = app_state_get_user_data();
  if (!data) {
    return;
  }

  const GRect layer_bounds = layer_get_bounds_by_value(layer);

  // Fill the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &layer_bounds);

  // Draw the text
  prv_draw_text(ctx, &layer_bounds);

  // Create the mask and start recording
  const bool transparent = false;
  GDrawMask *mask = graphics_context_mask_create(ctx, transparent);
  graphics_context_mask_record(ctx, mask);

  // The number of milliseconds it should take each of the shapes to make a full revolution
  const uint16_t full_revolution_time_ms = 4000;

  // Use the current system time to calculate the current animation progress
  time_t system_time_seconds;
  uint16_t system_time_ms;
  sys_get_time_ms(&system_time_seconds, &system_time_ms);

  const uint16_t current_time_progress_ms =
    (uint16_t)((system_time_seconds % (full_revolution_time_ms / MS_PER_SECOND)) * MS_PER_SECOND +
      (system_time_ms % MS_PER_SECOND));

  const AnimationProgress animation_progress =
    current_time_progress_ms * ANIMATION_NORMALIZED_MAX / full_revolution_time_ms;

  const GColor mask_colors[3] = {
    GColorLightGray,
    GColorDarkGray,
    GColorBlack
  };
  const unsigned int num_mask_levels = ARRAY_LENGTH(mask_colors);

  const int16_t shape_width =
    (int16_t)(MIN(layer_bounds.size.w, layer_bounds.size.h) / 2);
  const GSize shape_size = GSize(shape_width, shape_width);

  for (unsigned int i = 0; i < num_mask_levels; i++) {
    // Calculate the angle of the mask shape using the current animation progress
    // Offset the angle to space each of the mask shapes equally apart
    const uint32_t starting_angle = (i * TRIG_MAX_ANGLE / num_mask_levels);
    const int32_t progress_angle_delta =
      animation_progress * TRIG_MAX_ANGLE / ANIMATION_NORMALIZED_MAX;
    const int32_t current_angle = normalize_angle(starting_angle + progress_angle_delta);

    // Set the color to fill, progressing through each of the mask levels
    graphics_context_set_fill_color(ctx, mask_colors[i]);

    // Fill the mask shape
    prv_fill_mask_shape(ctx, &layer_bounds, &shape_size, current_angle);
  }

  // Activate the mask and fill the entire layer with a red rectangle
  graphics_context_mask_use(ctx, mask);

  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_rect(ctx, &layer_bounds);

  graphics_context_mask_destroy(ctx, mask);
}

static void prv_refresh_timer_callback(void *context) {
  GDrawMaskDemoData *data = context;
  layer_mark_dirty(window_get_root_layer(&data->window));
  app_timer_register(ANIMATION_TARGET_FRAME_INTERVAL_MS, prv_refresh_timer_callback, data);
}

static void prv_window_load(Window *window) {
  GDrawMaskDemoData *data = window_get_user_data(window);
  Layer *window_root_layer = window_get_root_layer(window);
  layer_set_update_proc(window_root_layer, prv_layer_update_proc);

  app_timer_register(ANIMATION_TARGET_FRAME_INTERVAL_MS, prv_refresh_timer_callback, data);
}

static void handle_init(void) {
  GDrawMaskDemoData *data = app_zalloc_check(sizeof(*data));
  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("GDrawMask Demo"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers){
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);

  app_light_enable(true);
}

static void handle_deinit(void) {
  app_light_enable(false);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd *gdrawmask_demo_get_app_info() {
  static const PebbleProcessMdSystem gdrawmask_demo_app_info = {
    .common.main_func = s_main,
    .name = "GDrawMask Demo"
  };
  return (const PebbleProcessMd*) &gdrawmask_demo_app_info;
}
