/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bouncing_box.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gtypes.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"

static const int TARGET_FPS = 20;
static const int PIXEL_SPEED_PER_FRAME = 4;

typedef struct AppData {
  Window window;

  GRect box_rect;
  int x_velocity;
  int y_velocity;

  uint8_t color;
} AppData;

static void prv_change_color(AppData *data) {
  data->color++; // next RGB value (in lower 6 bits, might overflow to most significant 2 bits)
  data->color |= (uint8_t)0b11000000; // make sure color is always 100% opaque
}

static void prv_move_rect(AppData *data) {
  data->box_rect.origin.x += (data->x_velocity * PIXEL_SPEED_PER_FRAME);

  if (data->box_rect.origin.x <= 0 ||
      data->box_rect.origin.x + data->box_rect.size.w > data->window.layer.bounds.size.w) {
    data->x_velocity = data->x_velocity * -1;

    prv_change_color(data);
  }

  data->box_rect.origin.y += (data->y_velocity * PIXEL_SPEED_PER_FRAME);

  if (data->box_rect.origin.y <= 0 ||
      data->box_rect.origin.y + data->box_rect.size.h > data->window.layer.bounds.size.h) {
    data->y_velocity = data->y_velocity * -1;

    prv_change_color(data);
  }
}

static void layer_update_proc(Layer *layer, GContext* ctx) {
  AppData *data = app_state_get_user_data();

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &layer->bounds);

  graphics_context_set_fill_color(ctx, (GColor)data->color);
  graphics_fill_rect(ctx, &data->box_rect);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &data->box_rect);
}


void prv_redraw_timer_cb(void *cb_data) {
  AppData *data = app_state_get_user_data();

  prv_move_rect(data);

  layer_mark_dirty(&data->window.layer);

  app_timer_register(1000 / TARGET_FPS, prv_redraw_timer_cb, NULL);
}

static void s_main(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Bouncing Box"));
  window_set_user_data(window, data);
  window_set_fullscreen(window, true);
  layer_set_update_proc(&window->layer, layer_update_proc);

  const bool animated = true;
  app_window_stack_push(window, animated);

  data->box_rect = GRect(10, 10, 40, 40);

  data->x_velocity = 1;
  data->y_velocity = 1;

  app_timer_register(33, prv_redraw_timer_cb, NULL);

  app_event_loop();
}

const PebbleProcessMd* bouncing_box_demo_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = s_main,
    .name = "Bouncing Box"
  };
  return (const PebbleProcessMd*) &s_app_info;
}

