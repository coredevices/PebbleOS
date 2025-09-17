/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include "applib/app.h"
#include "applib/app_message/app_message.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "apps/system_app_ids.h"
#include "drivers/hrm/as7000/as7000.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_info.h"
#include "mfg/mfg_serials.h"
#include "process_state/app_state/app_state.h"
#include "services/common/hrm/hrm_manager.h"
#include "system/passert.h"

#define BPM_STRING_LEN 10

typedef enum {
  AppMessageKey_Status = 1,

  AppMessageKey_HeartRate = 10,
  AppMessageKey_Confidence = 11,
  AppMessageKey_Current = 12,
  AppMessageKey_TIA = 13,
  AppMessageKey_PPG = 14,
  AppMessageKey_AccelData = 15,
  AppMessageKey_SerialNumber = 16,
  AppMessageKey_Model = 17,
  AppMessageKey_HRMProtocolVersionMajor = 18,
  AppMessageKey_HRMProtocolVersionMinor = 19,
  AppMessageKey_HRMSoftwareVersionMajor = 20,
  AppMessageKey_HRMSoftwareVersionMinor = 21,
  AppMessageKey_HRMApplicationID = 22,
  AppMessageKey_HRMHardwareRevision = 23,
} AppMessageKey;

typedef enum {
  AppStatus_Stopped = 0,
  AppStatus_Enabled_1HZ = 1,
} AppStatus;

typedef struct {
  HRMSessionRef session;
  EventServiceInfo hrm_event_info;

  Window window;
  TextLayer bpm_text_layer;
  TextLayer quality_text_layer;

  char bpm_string[BPM_STRING_LEN];

  bool ready_to_send;
  DictionaryIterator *out_iter;
} AppData;

static char *prv_get_quality_string(HRMQuality quality) {
  switch (quality) {
    case HRMQuality_NoAccel:
      return "No Accel Data";
    case HRMQuality_OffWrist:
      return "Off Wrist";
    case HRMQuality_NoSignal:
      return "No Signal";
    case HRMQuality_Worst:
      return "Worst";
    case HRMQuality_Poor:
      return "Poor";
    case HRMQuality_Acceptable:
      return "Acceptable";
    case HRMQuality_Good:
      return "Good";
    case HRMQuality_Excellent:
      return "Excellent";
  }
  WTF;
}

static char *prv_translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

static void prv_send_msg(void) {
  AppData *app_data = app_state_get_user_data();

  AppMessageResult result = app_message_outbox_send();
  if (result == APP_MSG_OK) {
    app_data->ready_to_send = false;
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "Error sending message: %s", prv_translate_error(result));
  }
}

static void prv_send_status_and_version(void) {
  AppData *app_data = app_state_get_user_data();
  PBL_LOG(LOG_LEVEL_DEBUG, "Sending status and version to mobile app");

  AppMessageResult result = app_message_outbox_begin(&app_data->out_iter);
  if (result != APP_MSG_OK) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Failed to begin outbox - reason %i %s",
            result, prv_translate_error(result));
    return;
  }

  dict_write_uint8(app_data->out_iter, AppMessageKey_Status, AppStatus_Enabled_1HZ);

#if CAPABILITY_HAS_BUILTIN_HRM
  if (mfg_info_is_hrm_present()) {
    AS7000InfoRecord hrm_info = {};
    as7000_get_version_info(HRM, &hrm_info);
    dict_write_uint8(app_data->out_iter, AppMessageKey_HRMProtocolVersionMajor,
                     hrm_info.protocol_version_major);
    dict_write_uint8(app_data->out_iter, AppMessageKey_HRMProtocolVersionMinor,
                     hrm_info.protocol_version_minor);
    dict_write_uint8(app_data->out_iter, AppMessageKey_HRMSoftwareVersionMajor,
                     hrm_info.sw_version_major);
    dict_write_uint8(app_data->out_iter, AppMessageKey_HRMSoftwareVersionMinor,
                     hrm_info.sw_version_minor);
    dict_write_uint8(app_data->out_iter, AppMessageKey_HRMApplicationID,
                     hrm_info.application_id);
    dict_write_uint8(app_data->out_iter, AppMessageKey_HRMHardwareRevision,
                     hrm_info.hw_revision);
  }
#endif

  char serial_number_buffer[MFG_SERIAL_NUMBER_SIZE + 1];
  mfg_info_get_serialnumber(serial_number_buffer, sizeof(serial_number_buffer));
  dict_write_data(app_data->out_iter, AppMessageKey_SerialNumber,
                  (uint8_t*) serial_number_buffer, sizeof(serial_number_buffer));

#if IS_BIGBOARD
  WatchInfoColor watch_color = WATCH_INFO_MODEL_UNKNOWN;
#else
  WatchInfoColor watch_color = mfg_info_get_watch_color();
#endif // IS_BIGBOARD
  dict_write_uint32(app_data->out_iter, AppMessageKey_Model, watch_color);

  prv_send_msg();
}

static void prv_handle_hrm_data(PebbleEvent *e, void *context) {
  AppData *app_data = app_state_get_user_data();

  if (e->type == PEBBLE_HRM_EVENT) {
    PebbleHRMEvent *hrm = &e->hrm;

    // Save HRMEventBPM data and send when we get the current into.
    static uint8_t bpm = 0;
    static uint8_t bpm_quality = 0;
    static uint16_t led_current = 0;

    if (hrm->event_type == HRMEvent_BPM) {
      snprintf(app_data->bpm_string, sizeof(app_data->bpm_string), "%"PRIu8" BPM", hrm->bpm.bpm);
      text_layer_set_text(&app_data->quality_text_layer, prv_get_quality_string(hrm->bpm.quality));
      layer_mark_dirty(&app_data->window.layer);

      bpm = hrm->bpm.bpm;
      bpm_quality = hrm->bpm.quality;
    } else if (hrm->event_type == HRMEvent_LEDCurrent) {
      led_current = hrm->led.current_ua;
    } else if (hrm->event_type == HRMEvent_Diagnostics) {
      if (!app_data->ready_to_send) {
        return;
      }

      AppMessageResult result = app_message_outbox_begin(&app_data->out_iter);
      PBL_ASSERTN(result == APP_MSG_OK);

      if (bpm) {
        dict_write_uint8(app_data->out_iter, AppMessageKey_HeartRate, bpm);
        dict_write_uint8(app_data->out_iter, AppMessageKey_Confidence, bpm_quality);
      }

      if (led_current) {
        dict_write_uint16(app_data->out_iter, AppMessageKey_Current, led_current);
      }

      if (hrm->debug->ppg_data.num_samples) {
        HRMPPGData *d = &hrm->debug->ppg_data;
        dict_write_data(app_data->out_iter, AppMessageKey_TIA,
                        (uint8_t *)d->tia, d->num_samples * sizeof(d->tia[0]));
        dict_write_data(app_data->out_iter, AppMessageKey_PPG,
                        (uint8_t *)d->ppg, d->num_samples * sizeof(d->ppg[0]));
      }

      if (hrm->debug->ppg_data.tia[hrm->debug->ppg_data.num_samples - 1] == 0) {
        PBL_LOG_COLOR(LOG_LEVEL_DEBUG, LOG_COLOR_CYAN, "last PPG TIA sample is 0!");
      }

      if (hrm->debug->ppg_data.num_samples != 20) {
        PBL_LOG_COLOR(LOG_LEVEL_DEBUG, LOG_COLOR_CYAN, "Only got %"PRIu16" samples!",
                      hrm->debug->ppg_data.num_samples);
      }

      if (hrm->debug->accel_data.num_samples) {
        HRMAccelData *d = &hrm->debug->accel_data;
        dict_write_data(app_data->out_iter, AppMessageKey_AccelData,
                        (uint8_t *)d->data, d->num_samples * sizeof(d->data[0]));
      }

      PBL_LOG(LOG_LEVEL_DEBUG,
              "Sending message - bpm:%u quality:%u current:%u "
              "ppg_readings:%u accel_readings %"PRIu32,
              bpm,
              bpm_quality,
              led_current,
              hrm->debug->ppg_data.num_samples,
              hrm->debug->accel_data.num_samples);

      led_current = bpm = bpm_quality = 0;

      prv_send_msg();
    } else if (hrm->event_type == HRMEvent_SubscriptionExpiring) {
      PBL_LOG(LOG_LEVEL_INFO, "Got subscription expiring event");
      // Subscribe again if our subscription is expiring
      const uint32_t update_time_s = 1;
      app_data->session = sys_hrm_manager_app_subscribe(APP_ID_HRM_DEMO, update_time_s,
                                                        SECONDS_PER_HOUR, HRMFeature_BPM);
    }
  }
}

static void prv_enable_hrm(void) {
  AppData *app_data = app_state_get_user_data();

  app_data->hrm_event_info = (EventServiceInfo) {
    .type = PEBBLE_HRM_EVENT,
    .handler = prv_handle_hrm_data,
  };
  event_service_client_subscribe(&app_data->hrm_event_info);

  // TODO: Let the mobile app control this?
  const uint32_t update_time_s = 1;
  app_data->session = sys_hrm_manager_app_subscribe(
      APP_ID_HRM_DEMO, update_time_s, SECONDS_PER_HOUR,
      HRMFeature_BPM | HRMFeature_LEDCurrent | HRMFeature_Diagnostics);
}

static void prv_disable_hrm(void) {
  AppData *app_data = app_state_get_user_data();

  event_service_client_unsubscribe(&app_data->hrm_event_info);
  sys_hrm_manager_unsubscribe(app_data->session);
}

static void prv_handle_mobile_status_request(AppStatus status) {
  AppData *app_data = app_state_get_user_data();

  if (status == AppStatus_Stopped) {
    text_layer_set_text(&app_data->bpm_text_layer, "Paused");
    text_layer_set_text(&app_data->quality_text_layer, "Paused by mobile");
    prv_disable_hrm();
  } else {
    app_data->bpm_string[0] = '\0';
    text_layer_set_text(&app_data->bpm_text_layer, app_data->bpm_string);
    text_layer_set_text(&app_data->quality_text_layer, "Loading...");
    prv_enable_hrm();
  }
}

static void prv_message_received_cb(DictionaryIterator *iterator, void *context) {
  Tuple *status_tuple = dict_find(iterator, AppMessageKey_Status);

  if (status_tuple) {
    prv_handle_mobile_status_request(status_tuple->value->uint8);
  }
}

static void prv_message_sent_cb(DictionaryIterator *iterator, void *context) {
  AppData *app_data = app_state_get_user_data();

  app_data->ready_to_send = true;
}

static void prv_message_failed_cb(DictionaryIterator *iterator,
                               AppMessageResult reason, void *context) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Out message send failed - reason %i %s",
          reason, prv_translate_error(reason));
  AppData *app_data = app_state_get_user_data();
  app_data->ready_to_send = true;
}

static void prv_remote_notify_timer_cb(void *data) {
  prv_send_status_and_version();
}

static void prv_init(void) {
  AppData *app_data = app_malloc_check(sizeof(*app_data));
  *app_data = (AppData) {
    .session = (HRMSessionRef)app_data, // Use app data as session ref
    .ready_to_send = false,
  };
  app_state_set_user_data(app_data);

  Window *window = &app_data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  GRect bounds = window->layer.bounds;

  bounds.origin.y += 40;
  TextLayer *bpm_tl = &app_data->bpm_text_layer;
  text_layer_init(bpm_tl, &bounds);
  text_layer_set_font(bpm_tl, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(bpm_tl, GTextAlignmentCenter);
  text_layer_set_text(bpm_tl, app_data->bpm_string);
  layer_add_child(&window->layer, &bpm_tl->layer);

  bounds.origin.y += 35;
  TextLayer *quality_tl = &app_data->quality_text_layer;
  text_layer_init(quality_tl, &bounds);
  text_layer_set_font(quality_tl, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(quality_tl, GTextAlignmentCenter);
  text_layer_set_text(quality_tl, "Loading...");
  layer_add_child(&window->layer, &quality_tl->layer);

  const uint32_t inbox_size = 64;
  const uint32_t outbox_size = 256;
  AppMessageResult result = app_message_open(inbox_size, outbox_size);
  if (result != APP_MSG_OK) {
    PBL_LOG(LOG_LEVEL_ERROR, "Unable to open app message! %i %s",
            result, prv_translate_error(result));
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "Successfully opened app message");
  }

  if (!sys_hrm_manager_is_hrm_present()) {
    text_layer_set_text(quality_tl, "No HRM Present");
  } else {
    text_layer_set_text(quality_tl, "Loading...");
    prv_enable_hrm();
  }

  app_message_register_inbox_received(prv_message_received_cb);
  app_message_register_outbox_sent(prv_message_sent_cb);
  app_message_register_outbox_failed(prv_message_failed_cb);

  app_timer_register(1000, prv_remote_notify_timer_cb, NULL);

  app_window_stack_push(window, true);
}

static void prv_deinit(void) {
  AppData *app_data = app_state_get_user_data();
  sys_hrm_manager_unsubscribe(app_data->session);
}

static void prv_main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd* hrm_demo_get_app_info(void) {
  static const PebbleProcessMdSystem s_hrm_demo_app_info = {
    .name = "HRM Demo",
    .common.uuid = { 0xf8, 0x1b, 0x2a, 0xf8, 0x13, 0x0a, 0x11, 0xe6,
                     0x86, 0x9f, 0xa4, 0x5e, 0x60, 0xb9, 0x77, 0x3d },
    .common.main_func = &prv_main,
  };
  // Only show in launcher if HRM is present
  return (sys_hrm_manager_is_hrm_present()) ? (const PebbleProcessMd*)&s_hrm_demo_app_info : NULL;
}
