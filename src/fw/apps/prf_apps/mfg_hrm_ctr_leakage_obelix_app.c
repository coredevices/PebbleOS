/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_hrm_ctr_leakage_obelix_app.h"

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "drivers/accel.h"
#include "drivers/hrm.h"
#include "drivers/hrm/gh3x2x/gh3x2x.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "mfg/mfg_info.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "process_management/process_manager.h"
#include "services/common/evented_timer.h"
#include "services/common/hrm/hrm_manager.h"
#include "util/bitset.h"
#include "util/size.h"
#include "util/trig.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define STATUS_STRING_LEN 32
#define CTR_STRING_LEN 128
#define LEAKAGE_STRING_LEN 128

#define PPG_GR_CTR_THS         (28.0f)
#define PPG_IR_CTR_THS         (36.0f)
#define PPG_RED_CTR_THS        (36.0f)
#define PPG_GR_LEAK_THS        (1.03f)
#define PPG_IR_LEAK_THS        (0.8f)
#define PPG_RED_LEAK_THS       (0.8f)

typedef struct {
  Window window;
  EventServiceInfo hrm_event_info;

  TextLayer title_text_layer;
  TextLayer status_text_layer;
  TextLayer ctr_text_layer;
  TextLayer leak_text_layer;
  char status_string[STATUS_STRING_LEN];
  char ctr_string[CTR_STRING_LEN];
  char leak_string[LEAKAGE_STRING_LEN];
  HRMSessionRef hrm_session;
  bool test_type;
} AppData;

static void prv_handle_hrm_data(PebbleEvent *e, void *context) {
  AppData *app_data = app_state_get_user_data();

  if (e->type == PEBBLE_HRM_EVENT) {
    if (e->hrm.event_type == HRMEvent_CTR) {
      bool rst = (e->hrm.ctr->ctr[0] >= PPG_GR_CTR_THS) && (e->hrm.ctr->ctr[1] >= PPG_GR_CTR_THS) 
              && (e->hrm.ctr->ctr[2] >= PPG_IR_CTR_THS) && (e->hrm.ctr->ctr[3] >= PPG_IR_CTR_THS)
              && (e->hrm.ctr->ctr[4] >= PPG_RED_CTR_THS) && (e->hrm.ctr->ctr[5] >= PPG_RED_CTR_THS);
      memset(app_data->ctr_string, 0, CTR_STRING_LEN);
      snprintf(app_data->ctr_string, CTR_STRING_LEN,
              "CTR:(%s)\n%4d.%02d %4d.%02d %4d.%02d\n%4d.%02d %4d.%02d %4d.%02d", 
              rst?"PASS":"FAILED",
              (int)e->hrm.ctr->ctr[0], (int)(e->hrm.ctr->ctr[0]*100)%100, 
              (int)e->hrm.ctr->ctr[2], (int)(e->hrm.ctr->ctr[2]*100)%100, 
              (int)e->hrm.ctr->ctr[4], (int)(e->hrm.ctr->ctr[4]*100)%100, 
              (int)e->hrm.ctr->ctr[1], (int)(e->hrm.ctr->ctr[1]*100)%100, 
              (int)e->hrm.ctr->ctr[3], (int)(e->hrm.ctr->ctr[3]*100)%100, 
              (int)e->hrm.ctr->ctr[5], (int)(e->hrm.ctr->ctr[5]*100)%100);
      PBL_LOG(LOG_LEVEL_DEBUG, "%s", app_data->ctr_string);
    } else if (e->hrm.event_type == HRMEvent_Leakage) {
      bool rst = (e->hrm.leakage->leakage[0] <= PPG_GR_LEAK_THS) && (e->hrm.leakage->leakage[1] <= PPG_GR_LEAK_THS) 
              && (e->hrm.leakage->leakage[2] <= PPG_IR_LEAK_THS) && (e->hrm.leakage->leakage[3] <= PPG_IR_LEAK_THS)
              && (e->hrm.leakage->leakage[4] <= PPG_RED_LEAK_THS) && (e->hrm.leakage->leakage[5] <= PPG_RED_LEAK_THS);
      memset(app_data->leak_string, 0, LEAKAGE_STRING_LEN);
      snprintf(app_data->leak_string, LEAKAGE_STRING_LEN,
        "Leak:(%s)\n%4d.%02d %4d.%02d %4d.%02d\n%4d.%02d %4d.%02d %4d.%02d", 
              rst?"PASS":"FAILED",
              (int)e->hrm.leakage->leakage[0], (int)(e->hrm.leakage->leakage[0]*100)%100, 
              (int)e->hrm.leakage->leakage[2], (int)(e->hrm.leakage->leakage[2]*100)%100, 
              (int)e->hrm.leakage->leakage[4], (int)(e->hrm.leakage->leakage[4]*100)%100, 
              (int)e->hrm.leakage->leakage[1], (int)(e->hrm.leakage->leakage[1]*100)%100, 
              (int)e->hrm.leakage->leakage[3], (int)(e->hrm.leakage->leakage[3]*100)%100, 
              (int)e->hrm.leakage->leakage[5], (int)(e->hrm.leakage->leakage[5]*100)%100);
      PBL_LOG(LOG_LEVEL_DEBUG, "%s", app_data->leak_string);
    }

    layer_mark_dirty(&app_data->window.layer);
  }
}

static void prv_update_status(void* param) {
  layer_mark_dirty((Layer *)param);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();
  if (app_data->test_type) {
    gh3x2x_start_ft_ctr();
    snprintf(app_data->status_string, STATUS_STRING_LEN, "CTR Sampling...");
  } else {
    gh3x2x_start_ft_leakage();
    snprintf(app_data->status_string, STATUS_STRING_LEN, "Leak Sampling...");
  }
  app_data->test_type = !app_data->test_type;

  app_timer_register(10, prv_update_status, &app_data->window.layer);
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}

static void prv_handle_init(void) {
  AppData *data = task_zalloc(sizeof(*data));
  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  TextLayer *title = &data->title_text_layer;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "HRM TEST");
  layer_add_child(&window->layer, &title->layer);

  sniprintf(data->status_string, STATUS_STRING_LEN, "Press Sel to Start");

  snprintf(data->ctr_string, CTR_STRING_LEN, "CTR:--");
  snprintf(data->leak_string, LEAKAGE_STRING_LEN, "Leak:--");

  TextLayer *status = &data->status_text_layer;
  text_layer_init(status,
                  &GRect(5, 30, window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 30));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  text_layer_set_text(status, data->status_string);
  layer_add_child(&window->layer, &status->layer);

  TextLayer *leak = &data->leak_text_layer;
  text_layer_init(leak,
                  &GRect(5, 60, window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 140));
  text_layer_set_font(leak, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(leak, GTextAlignmentCenter);
  text_layer_set_text(leak, data->leak_string);
  layer_add_child(&window->layer, &leak->layer);

  TextLayer *ctr = &data->ctr_text_layer;
  text_layer_init(ctr,
                  &GRect(5, 140, window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 60));
  text_layer_set_font(ctr, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(ctr, GTextAlignmentCenter);
  text_layer_set_text(ctr, data->ctr_string);
  layer_add_child(&window->layer, &ctr->layer);

  data->hrm_event_info = (EventServiceInfo){
    .type = PEBBLE_HRM_EVENT,
    .handler = prv_handle_hrm_data,
  };
  event_service_client_subscribe(&data->hrm_event_info);

  // Use app data as session ref
  AppInstallId  app_id = 1;
  data->hrm_session = sys_hrm_manager_app_subscribe(app_id, 1, SECONDS_PER_HOUR,
                                                    HRMFeature_CTR | HRMFeature_Leakage);

  app_window_stack_push(window, true);
}

static void prv_handle_deinit(void) {
  AppData *data = app_state_get_user_data();
  event_service_client_unsubscribe(&data->hrm_event_info);
  sys_hrm_manager_unsubscribe(data->hrm_session);

  text_layer_deinit(&data->title_text_layer);
  text_layer_deinit(&data->status_text_layer);
  window_deinit(&data->window);
  app_free(data);
}

static void prv_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd* mfg_hrm_ctr_leakage_obelix_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &prv_main,
    .name = "MfgHRMCTRLeakageObelix",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
