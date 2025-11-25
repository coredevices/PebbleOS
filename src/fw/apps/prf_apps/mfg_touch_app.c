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

#define RECTR_ROW (5)
#define RECTR_COL (4)

#define TOUCH_SUPPORT_DEBUG    0

typedef struct {
  Window window;
  GRect rectr[RECTR_ROW * RECTR_COL];
  uint16_t rectr_radius;
  uint16_t rectr_corners_index;
  uint32_t touch_mark;
  EventServiceInfo event_info;
} AppData;

static void prv_update_proc(struct Layer *layer, GContext* ctx) {
  AppData *data = app_state_get_user_data();

  for (uint8_t i=0; i<RECTR_ROW * RECTR_COL; i++) {
    if (data->touch_mark & (1 << i)) {
      graphics_context_set_fill_color(ctx, GColorGreen);
      graphics_fill_round_rect(ctx, &data->rectr[i], data->rectr_radius, GCornersAll);
    }
  }
}

static void prv_touch_envent_handler(const TouchEvent *event, void *context) {
  AppData *data = app_state_get_user_data();
  uint16_t x = event->start_pos.x;
  uint16_t y = event->start_pos.y;

  int16_t offset_x = event->diff_pos.x;
  int16_t offset_y = event->diff_pos.y;

  uint8_t ind_x = (x+offset_x)/data->rectr[0].size.w;
  if (ind_x > RECTR_COL-1) ind_x = RECTR_COL-1;
  uint8_t ind_y = (y+offset_y)/data->rectr[0].size.h;
  if (ind_y > RECTR_ROW-1) ind_y = RECTR_ROW-1;
  uint8_t touch_id = ind_x * RECTR_ROW + ind_y;

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
  layer_mark_dirty(&data->window.layer);

  data->event_info = (EventServiceInfo) {
    .type = PEBBLE_TOUCH_EVENT,
    .handler = prv_handle_touch_event,
  };
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
