/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mic_demo.h"

#include "applib/app.h"
#include "applib/app_permissions.h"
#include "applib/mic_data_service.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include <pbl/logging/logging.h>

#include <inttypes.h>
#include <stdio.h>

// Exercises the Microphone API: SELECT toggles capture, UP toggles streaming to the phone. The
// screen shows the permission state, the RMS level of the last batch and the unobstructed height
// (which shrinks under the banner).

#define SAMPLES_PER_UPDATE (320) // 20 ms

typedef struct {
  Window window;
  TextLayer status_layer;
  TextLayer level_layer;
  char status_buffer[64];
  char level_buffer[48];
  uint32_t batches;
  uint32_t overruns;
} MicDemoAppData;

static void prv_update_status(MicDemoAppData *data, const char *text) {
  const AppPermissionState state = app_permission_get_state(AppPermission_Microphone);
  GRect unobstructed;
  layer_get_unobstructed_bounds(&data->window.layer, &unobstructed);
  snprintf(data->status_buffer, sizeof(data->status_buffer), "perm=%u h=%d\n%s", (unsigned)state,
           unobstructed.size.h, text);
  text_layer_set_text(&data->status_layer, data->status_buffer);
}

static void prv_data_handler(const int16_t *samples, uint32_t num_samples, bool overrun,
                             void *context) {
  MicDemoAppData *data = context;
  uint64_t acc = 0;
  for (uint32_t i = 0; i < num_samples; i++) {
    acc += (int32_t)samples[i] * (int32_t)samples[i];
  }
  // Integer square root of the mean square
  uint32_t mean = (uint32_t)(acc / num_samples);
  uint32_t rms = 0;
  for (uint32_t bit = 1u << 15; bit; bit >>= 1) {
    const uint32_t candidate = rms | bit;
    if (candidate * candidate <= mean) {
      rms = candidate;
    }
  }
  data->batches++;
  if (overrun) {
    data->overruns++;
  }
  snprintf(data->level_buffer, sizeof(data->level_buffer),
           "rms %" PRIu32 "\nbatch %" PRIu32 " drop %" PRIu32, rms, data->batches, data->overruns);
  text_layer_set_text(&data->level_layer, data->level_buffer);
  if ((data->batches % 50) == 0) {
    PBL_LOG_DBG("mic demo: %" PRIu32 " batches, rms %" PRIu32 ", overruns %" PRIu32, data->batches,
                rms, data->overruns);
  }
}

static void prv_stopped_handler(MicDataStopReason reason, void *context) {
  MicDemoAppData *data = context;
  static const char *const s_reasons[] = {
    [MicDataStopReasonStopped] = "stopped",
    [MicDataStopReasonFocusLost] = "focus lost",
    [MicDataStopReasonInterrupted] = "interrupted",
    [MicDataStopReasonPermissionRevoked] = "revoked",
    [MicDataStopReasonError] = "error",
    [MicDataStopReasonPhone] = "phone ended",
  };
  PBL_LOG_DBG("mic demo: capture stopped (%s)", s_reasons[reason]);
  prv_update_status(data, s_reasons[reason]);
}

static void prv_permission_changed(AppPermission permission, AppPermissionState state,
                                   void *context) {
  PBL_LOG_DBG("mic demo: permission %u -> %u", permission, state);
  prv_update_status(context, "permission changed");
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  MicDemoAppData *data = context;
  if (mic_data_service_is_active()) {
    mic_data_service_unsubscribe();
    prv_update_status(data, "idle");
    return;
  }
  const MicDataStartResult rv = mic_data_service_subscribe(
      SAMPLES_PER_UPDATE,
      (MicDataHandlers){.data = prv_data_handler, .stopped = prv_stopped_handler}, data);
  static const char *const s_results[] = {
    [MicDataStartOk] = "listening",
    [MicDataStartErrNotDeclared] = "not declared",
    [MicDataStartErrPermissionDenied] = "denied",
    [MicDataStartErrBusy] = "busy",
    [MicDataStartErrNotForeground] = "not foreground",
    [MicDataStartErrInvalidArgs] = "invalid args",
    [MicDataStartErrNoMemory] = "no memory",
    [MicDataStartErrWatchface] = "watchface",
  };
  PBL_LOG_DBG("mic demo: subscribe -> %s", s_results[rv]);
  prv_update_status(data, s_results[rv]);
}

static void prv_stream_started(void *context) {
  PBL_LOG_DBG("mic demo: phone accepted the stream");
  prv_update_status(context, "streaming");
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  MicDemoAppData *data = context;
  if (mic_stream_to_phone_is_active()) {
    mic_stream_to_phone_stop();
    prv_update_status(data, "idle");
    return;
  }
  const MicDataStartResult rv = mic_stream_to_phone_start(
      (MicStreamHandlers){.started = prv_stream_started, .stopped = prv_stopped_handler}, data);
  PBL_LOG_DBG("mic demo: stream -> %u", rv);
  prv_update_status(data, (rv == MicDataStartOk) ? "waiting for phone" : "stream refused");
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
}

static void prv_unobstructed_did_change(void *context) {
  MicDemoAppData *data = context;
  prv_update_status(data, mic_data_service_is_active() ? "listening" : "idle");
}

static void prv_window_load(Window *window) {
  MicDemoAppData *data = window_get_user_data(window);
  const GRect bounds = window->layer.bounds;

  text_layer_init(&data->status_layer, &GRect(0, 10, bounds.size.w, 60));
  text_layer_set_text_alignment(&data->status_layer, GTextAlignmentCenter);
  layer_add_child(&window->layer, &data->status_layer.layer);

  text_layer_init(&data->level_layer, &GRect(0, 70, bounds.size.w, 60));
  text_layer_set_text_alignment(&data->level_layer, GTextAlignmentCenter);
  layer_add_child(&window->layer, &data->level_layer.layer);

  prv_update_status(data, "SELECT: capture UP: stream");
}

static void prv_handle_init(void) {
  MicDemoAppData *data = app_zalloc_check(sizeof(*data));
  app_state_set_user_data(data);

  window_init(&data->window, "Mic Demo");
  window_set_user_data(&data->window, data);
  window_set_click_config_provider_with_context(&data->window, prv_click_config_provider, data);
  window_set_window_handlers(&data->window, &(WindowHandlers){.load = prv_window_load});

  app_permission_service_subscribe(prv_permission_changed, data);
  app_unobstructed_area_service_subscribe(
      (UnobstructedAreaHandlers){.did_change = prv_unobstructed_did_change}, data);

  app_window_stack_push(&data->window, true /* animated */);
}

static void prv_handle_deinit(void) {
  MicDemoAppData *data = app_state_get_user_data();
  mic_data_service_unsubscribe();
  mic_stream_to_phone_stop();
  app_permission_service_unsubscribe();
  app_unobstructed_area_service_unsubscribe();
  app_free(data);
}

static void s_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd *mic_demo_get_info(void) {
  static const PebbleProcessMdSystem s_mic_demo_info = {
    .common.main_func = s_main,
    .name = "Mic Demo",
  };
  return (const PebbleProcessMd *)&s_mic_demo_info;
}
