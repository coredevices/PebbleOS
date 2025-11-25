/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_hrm_app.h"

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "drivers/accel.h"
#include "drivers/hrm.h"
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

#if CAPABILITY_HAS_BUILTIN_HRM

#define STATUS_STRING_LEN 32

typedef struct {
  Window window;
  EventServiceInfo hrm_event_info;

  TextLayer title_text_layer;
  TextLayer status_text_layer;
  char status_string[STATUS_STRING_LEN];
  HRMSessionRef hrm_session;
} AppData;

static void prv_handle_hrm_data(PebbleEvent *e, void *context) {
  AppData *app_data = app_state_get_user_data();

  if (e->type == PEBBLE_HRM_EVENT) {
    snprintf(app_data->status_string, STATUS_STRING_LEN,
             "TIA: %"PRIu16"\nLED: %"PRIu16" mA", e->hrm.led.tia, e->hrm.led.current_ua);
    layer_mark_dirty(&app_data->window.layer);
  }
}

static void prv_handle_init(void) {
  const bool has_hrm = mfg_info_is_hrm_present();

  AppData *data = task_zalloc(sizeof(*data));
  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  TextLayer *title = &data->title_text_layer;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "HRM TEST");
  layer_add_child(&window->layer, &title->layer);

  if (has_hrm) {
    sniprintf(data->status_string, STATUS_STRING_LEN, "Starting...");
  } else {
    sniprintf(data->status_string, STATUS_STRING_LEN, "Not an HRM device");
  }
  TextLayer *status = &data->status_text_layer;
  text_layer_init(status,
                  &GRect(5, 40, window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 40));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  text_layer_set_text(status, data->status_string);
  layer_add_child(&window->layer, &status->layer);


  if (has_hrm) {
    data->hrm_event_info = (EventServiceInfo){
      .type = PEBBLE_HRM_EVENT,
      .handler = prv_handle_hrm_data,
    };
    event_service_client_subscribe(&data->hrm_event_info);

    // Use app data as session ref
    AppInstallId  app_id = 1;
    data->hrm_session = sys_hrm_manager_app_subscribe(app_id, 1, SECONDS_PER_HOUR,
                                                      HRMFeature_LEDCurrent);
  }

  app_window_stack_push(window, true);
}

static void prv_handle_deinit(void) {
  AppData *data = app_state_get_user_data();
  event_service_client_unsubscribe(&data->hrm_event_info);
  if (mfg_info_is_hrm_present()) {
    sys_hrm_manager_unsubscribe(data->hrm_session);
  }

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

const PebbleProcessMd* mfg_hrm_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &prv_main,
    .name = "MfgHRM",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

#endif // CAPABILITY_HAS_BUILTIN_HRM
