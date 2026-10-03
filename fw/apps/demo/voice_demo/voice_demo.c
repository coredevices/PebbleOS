/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "applib/app.h"
#include "applib/app_logging.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "applib/voice/dictation_session.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include <pbl/drivers/rtc.h>

#define TRANSCRIPTION_BUFFER_SIZE 512
#define MARGIN                    8
#define INFO_HEIGHT               48

typedef struct {
  Window window;
  TextLayer info_layer;
  TextLayer result_layer;
  DictationSession *session;
  bool confirm;
  bool error_dialogs;
  uint32_t sessions;
  RtcTicks started_at;
  char info[96];
  char result[TRANSCRIPTION_BUFFER_SIZE + 32];
} VoiceDemoData;

static const char *prv_status_name(DictationSessionStatus status) {
  switch (status) {
    case DictationSessionStatusSuccess:
      return "Success";
    case DictationSessionStatusFailureTranscriptionRejected:
      return "Rejected";
    case DictationSessionStatusFailureTranscriptionRejectedWithError:
      return "Rejected (error)";
    case DictationSessionStatusFailureSystemAborted:
      return "Aborted";
    case DictationSessionStatusFailureNoSpeechDetected:
      return "No speech";
    case DictationSessionStatusFailureConnectivityError:
      return "Connectivity";
    case DictationSessionStatusFailureDisabled:
      return "Disabled";
    case DictationSessionStatusFailureInternalError:
      return "Internal error";
    case DictationSessionStatusFailureRecognizerError:
      return "Recognizer error";
  }
  return "Unknown";
}

static void prv_update_info(VoiceDemoData *data) {
  snprintf(data->info, sizeof(data->info), "Sessions: %" PRIu32 "\nConfirm: %s  Errors: %s",
           data->sessions, data->confirm ? "on" : "off", data->error_dialogs ? "on" : "off");
  text_layer_set_text(&data->info_layer, data->info);
}

static void prv_dictation_cb(DictationSession *session, DictationSessionStatus status,
                             char *transcription, void *context) {
  VoiceDemoData *data = context;
  const uint32_t elapsed_ms =
      (uint32_t)((rtc_get_ticks() - data->started_at) * 1000 / RTC_TICKS_HZ);

  if (status == DictationSessionStatusSuccess) {
    snprintf(data->result, sizeof(data->result), "\"%s\"\n(%" PRIu32 " ms)", transcription,
             elapsed_ms);
  } else {
    snprintf(data->result, sizeof(data->result), "%s (%d)\n(%" PRIu32 " ms)",
             prv_status_name(status), (int)status, elapsed_ms);
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Dictation status %d after %" PRIu32 " ms", (int)status, elapsed_ms);
  text_layer_set_text(&data->result_layer, data->result);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  VoiceDemoData *data = context;
  data->sessions++;
  data->started_at = rtc_get_ticks();
  prv_update_info(data);
  text_layer_set_text(&data->result_layer, "Listening...");
  const DictationSessionStatus status = dictation_session_start(data->session);
  if (status != DictationSessionStatusSuccess) {
    snprintf(data->result, sizeof(data->result), "Start failed: %s (%d)", prv_status_name(status),
             (int)status);
    text_layer_set_text(&data->result_layer, data->result);
  }
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  VoiceDemoData *data = context;
  data->confirm = !data->confirm;
  dictation_session_enable_confirmation(data->session, data->confirm);
  prv_update_info(data);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  VoiceDemoData *data = context;
  data->error_dialogs = !data->error_dialogs;
  dictation_session_enable_error_dialogs(data->session, data->error_dialogs);
  prv_update_info(data);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_init(void) {
  VoiceDemoData *data = app_zalloc_check(sizeof(*data));
  app_state_set_user_data(data);

  data->confirm = true;
  data->error_dialogs = true;
  data->session = dictation_session_create(TRANSCRIPTION_BUFFER_SIZE, prv_dictation_cb, data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Voice Demo"));
  window_set_click_config_provider_with_context(window, prv_click_config_provider, data);

  const GRect bounds = window->layer.bounds;
  const GRect info_frame = GRect(MARGIN, MARGIN, bounds.size.w - 2 * MARGIN, INFO_HEIGHT);
  const GRect result_frame = GRect(MARGIN, MARGIN + INFO_HEIGHT, bounds.size.w - 2 * MARGIN,
                                   bounds.size.h - INFO_HEIGHT - 2 * MARGIN);

  text_layer_init(&data->info_layer, &info_frame);
  text_layer_set_font(&data->info_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(&data->info_layer, GTextAlignmentCenter);
  layer_add_child(&window->layer, &data->info_layer.layer);

  text_layer_init(&data->result_layer, &result_frame);
  text_layer_set_font(&data->result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(&data->result_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(&data->result_layer, GTextOverflowModeWordWrap);
  layer_add_child(&window->layer, &data->result_layer.layer);

  if (data->session) {
    text_layer_set_text(&data->result_layer, "Press SELECT to dictate");
  } else {
    text_layer_set_text(&data->result_layer, "Dictation not available");
  }
  prv_update_info(data);

  app_window_stack_push(window, true);
}

static void prv_deinit(void) {
  VoiceDemoData *data = app_state_get_user_data();
  dictation_session_destroy(data->session);
}

static void prv_main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd *voice_demo_get_app_info(void) {
  static const PebbleProcessMdSystem s_voice_demo_app_info = {
    .name = "Voice Demo",
    .common.uuid =
        {0x5c, 0x0e, 0x6a, 0x41, 0x2b, 0x8d, 0x4f, 0x2e, 0x9a, 0x37, 0x1d, 0xc4, 0x80, 0x6b, 0x52,
         0xf3},
    .common.main_func = &prv_main,
  };
  return (const PebbleProcessMd *)&s_voice_demo_app_info;
}
