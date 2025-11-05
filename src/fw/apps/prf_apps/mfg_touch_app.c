/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */


#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_mode/mfg_factory_mode.h"
#include "mfg/results_ui.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "services/common/light.h"
#include "services/common/touch/touch.h"
#include "services/common/touch/touch_event.h"
#include "services/common/touch/touch_client.h"
#include "util/trig.h"

#if PBL_ROUND
#define CIRCLE_ROWS (5)
#define CIRCLE_COLS (5)
#else
#define RECTR_ROW (5)
#define RECTR_COL (4)
#endif

#define TOUCH_SUPPORT_DEBUG    0

typedef struct {
  Window window;
  uint32_t touch_mark;
  EventServiceInfo event_info;
#if PBL_ROUND
  GPoint circle_centers[CIRCLE_ROWS * CIRCLE_COLS];
  uint16_t circle_radius;
  uint8_t num_circles;
#else
  GRect rectr[RECTR_ROW * RECTR_COL];
  uint16_t rectr_radius;
  uint16_t rectr_corners_index;
#endif
} AppData;

static void prv_update_proc(struct Layer *layer, GContext* ctx) {
  AppData *data = app_state_get_user_data();

#if PBL_ROUND
  for (uint8_t i=0; i < data->num_circles; i++) {
    if (data->touch_mark & (1 << i)) {
      graphics_context_set_fill_color(ctx, GColorGreen);
      graphics_fill_circle(ctx, data->circle_centers[i], data->circle_radius);
    }
  }
#else
  for (uint8_t i=0; i<RECTR_ROW * RECTR_COL; i++) {
    if (data->touch_mark & (1 << i)) {
      graphics_context_set_fill_color(ctx, GColorGreen);
      graphics_fill_round_rect(ctx, &data->rectr[i], data->rectr_radius, GCornersAll);
    }
  }
#endif
}

static void prv_touch_envent_handler(const TouchEvent *event, void *context) {
  AppData *data = app_state_get_user_data();
  uint16_t x = event->start_pos.x;
  uint16_t y = event->start_pos.y;

  int16_t offset_x = event->diff_pos.x;
  int16_t offset_y = event->diff_pos.y;

  uint8_t touch_id;

#if PBL_ROUND
  // Find the closest circle to the touch point
  int16_t touch_x = x + offset_x;
  int16_t touch_y = y + offset_y;
  uint32_t min_dist_sq = 0xFFFFFFFF;
  touch_id = 0;

  for (uint8_t i = 0; i < data->num_circles; i++) {
    int16_t dx = touch_x - data->circle_centers[i].x;
    int16_t dy = touch_y - data->circle_centers[i].y;
    uint32_t dist_sq = dx * dx + dy * dy;
    if (dist_sq < min_dist_sq) {
      min_dist_sq = dist_sq;
      touch_id = i;
    }
  }
#else
  uint8_t ind_x = (x+offset_x)/data->rectr[0].size.w;
  if (ind_x > RECTR_COL-1) ind_x = RECTR_COL-1;
  uint8_t ind_y = (y+offset_y)/data->rectr[0].size.h;
  if (ind_y > RECTR_ROW-1) ind_y = RECTR_ROW-1;
  touch_id = ind_x * RECTR_ROW + ind_y;
#endif

  data->touch_mark |= 1<<touch_id;
#if TOUCH_SUPPORT_DEBUG
  PBL_LOG(LOG_LEVEL_INFO, "start_x:%d start_y:%d off_x:%d off_y:%d", x, y, offset_x, offset_y);
  PBL_LOG(LOG_LEVEL_INFO, "x:%d y:%d id:%d", (x+offset_x), (y+offset_y), touch_id);
#endif
  layer_mark_dirty(&data->window.layer);
}

static void prv_handle_touch_event(PebbleEvent *e, void *context) {
  AppData *data = app_state_get_user_data();

  if (e->type == PEBBLE_TOUCH_EVENT) {
    PebbleTouchEvent *touch = &e->touch;
    touch_dispatch_touch_events(touch->touch_idx, prv_touch_envent_handler, context);
  }
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  Layer *layer = window_get_root_layer(window);
  layer_set_update_proc(layer, prv_update_proc);
  app_window_stack_push(window, true /* Animated */);
  GContext* context = app_state_get_graphics_context();

#if PBL_ROUND
  // Create honeycomb-like pattern with circles that touch but don't overlap
  uint16_t display_radius = PBL_DISPLAY_WIDTH / 2;

  // Calculate circle radius to fit 5 circles across the diameter
  // Spacing: circles just touch, so horizontal spacing = 2 * radius
  // For 5 circles across: 5 * (2 * radius) should fit in display width
  data->circle_radius = PBL_DISPLAY_WIDTH / 10;

  // For honeycomb packing, vertical spacing is 2 * radius * sqrt(3)/2 ≈ 1.732 * radius
  // We'll approximate sqrt(3)/2 ≈ 0.866 using integer math: 866/1000
  int16_t vertical_spacing = (data->circle_radius * 1732) / 1000;
  int16_t horizontal_spacing = 2 * data->circle_radius;

  // Calculate grid offset to center the pattern
  int16_t grid_width = (CIRCLE_COLS - 1) * horizontal_spacing;
  int16_t grid_height = (CIRCLE_ROWS - 1) * vertical_spacing;
  int16_t start_x = (PBL_DISPLAY_WIDTH - grid_width) / 2;
  int16_t start_y = (PBL_DISPLAY_HEIGHT - grid_height) / 2;

  data->num_circles = 0;

  // Create a honeycomb pattern: odd rows are offset by radius
  for (uint8_t row = 0; row < CIRCLE_ROWS; row++) {
    for (uint8_t col = 0; col < CIRCLE_COLS; col++) {
      int16_t x = start_x + col * horizontal_spacing;
      int16_t y = start_y + row * vertical_spacing;

      // Offset odd rows to create honeycomb pattern
      if (row % 2 == 1) {
        x += data->circle_radius;
      }

      // Only add circles that are within the display radius
      int16_t dx = x - PBL_DISPLAY_WIDTH / 2;
      int16_t dy = y - PBL_DISPLAY_HEIGHT / 2;
      uint32_t dist_sq = dx * dx + dy * dy;
      uint32_t max_dist_sq = (display_radius - data->circle_radius) * (display_radius - data->circle_radius);

      if (dist_sq <= max_dist_sq && data->num_circles < CIRCLE_ROWS * CIRCLE_COLS) {
        data->circle_centers[data->num_circles].x = x;
        data->circle_centers[data->num_circles].y = y;
        data->num_circles++;
      }
    }
  }

  // Draw the circle outlines
  graphics_context_set_stroke_color(context, GColorBlack);
  for (uint8_t i = 0; i < data->num_circles; i++) {
    graphics_draw_circle(context, data->circle_centers[i], data->circle_radius);
  }
#else
  //draw rectrs
  data->rectr_radius = 5;
  data->rectr_corners_index = GCornersAll;
  for (uint8_t x=0; x<RECTR_COL; x++) {
    for (uint8_t y=0; y<RECTR_ROW; y++) {
      data->rectr[RECTR_ROW * x + y].origin.x = (PBL_DISPLAY_WIDTH/RECTR_COL) * x;
      data->rectr[RECTR_ROW * x + y].origin.y = (PBL_DISPLAY_HEIGHT/RECTR_ROW) * y;
      data->rectr[RECTR_ROW * x + y].size.w = PBL_DISPLAY_WIDTH/RECTR_COL;
      data->rectr[RECTR_ROW * x + y].size.h = PBL_DISPLAY_HEIGHT/RECTR_ROW;
      graphics_context_set_stroke_color(context, GColorBlack);
      graphics_draw_round_rect(context, &data->rectr[RECTR_ROW * x + y], data->rectr_radius);
    }
  }
#endif
  layer_mark_dirty(&data->window.layer);

  data->event_info = (EventServiceInfo) {
    .type = PEBBLE_TOUCH_EVENT,
    .handler = prv_handle_touch_event,
  };
  touch_reset();
  event_service_client_subscribe(&data->event_info);
}

static void s_main(void) {
  light_enable(true);

  prv_handle_init();

  app_event_loop();

  light_enable(false);
}

const PebbleProcessMd* mfg_touch_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: a53e7d1c-d2ee-4592-96b9-5d33a46237db
    .common.uuid = { 0xa5, 0x3e, 0x7d, 0x1c, 0xd2, 0xee, 0x45, 0x92,
                     0x96, 0xb9, 0x5d, 0x33, 0xa4, 0x62, 0x37, 0xdb },
    .name = "MfgTouch",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
