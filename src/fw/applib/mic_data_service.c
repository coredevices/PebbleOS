/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/mic_data_service.h"
#include "applib/mic_data_service_private.h"

#include "applib/applib_malloc.auto.h"
#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/mic_capture/mic_capture_service.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"

static MicDataServiceState *prv_get_state(void) {
  if (pebble_task_get_current() != PebbleTask_App) {
    return NULL;
  }
  return app_state_get_mic_data_service_state();
}

static void prv_teardown(MicDataServiceState *state) {
  event_service_client_unsubscribe(&state->event_info);
  applib_free(state->buffer);
  state->buffer = NULL;
  state->handlers = (MicDataHandlers){};
  state->stream_handlers = (MicStreamHandlers){};
  state->context = NULL;
  state->active = false;
  state->streaming = false;
}

static MicDataStopReason prv_map_stop_reason(uint8_t kernel_reason) {
  switch ((MicCaptureStopReason)kernel_reason) {
    case MicCaptureStopReasonStopped:
      return MicDataStopReasonStopped;
    case MicCaptureStopReasonFocusLost:
      return MicDataStopReasonFocusLost;
    case MicCaptureStopReasonPreempted:
      return MicDataStopReasonInterrupted;
    case MicCaptureStopReasonPermissionRevoked:
      return MicDataStopReasonPermissionRevoked;
    case MicCaptureStopReasonPhone:
      return MicDataStopReasonPhone;
    case MicCaptureStopReasonAppExit:
    case MicCaptureStopReasonError:
      break;
  }
  return MicDataStopReasonError;
}

static void prv_handle_event(PebbleEvent *e, void *context) {
  MicDataServiceState *state = context;
  if (!state->active) {
    return;
  }

  if (e->mic_capture.type == MicCaptureEventStopped) {
    const MicDataStopReason reason = prv_map_stop_reason(e->mic_capture.stop_reason);
    const MicDataStoppedHandler stopped =
        state->streaming ? state->stream_handlers.stopped : state->handlers.stopped;
    void *ctx = state->context;
    prv_teardown(state);
    if (stopped) {
      stopped(reason, ctx);
    }
    return;
  }
  if (e->mic_capture.type == MicCaptureEventStarted) {
    if (state->streaming && state->stream_handlers.started) {
      state->stream_handlers.started(state->context);
    }
    return;
  }
  if (state->streaming) {
    return;
  }

  bool overrun = e->mic_capture.overrun;
  while (state->active && (sys_mic_capture_get_available() >= state->samples_per_update)) {
    const uint32_t num_samples = sys_mic_capture_read(state->buffer, state->samples_per_update);
    if (num_samples == 0) {
      break;
    }
    state->handlers.data(state->buffer, num_samples, overrun, state->context);
    overrun = false;
  }
}

MicDataStartResult mic_data_service_subscribe(uint32_t samples_per_update, MicDataHandlers handlers,
                                              void *context) {
  MicDataServiceState *state = prv_get_state();
  if (!state) {
    return MicDataStartErrNotForeground;
  }
  if (!handlers.data || (samples_per_update < MIC_DATA_MIN_SAMPLES_PER_UPDATE) ||
      (samples_per_update > MIC_DATA_MAX_SAMPLES_PER_UPDATE)) {
    return MicDataStartErrInvalidArgs;
  }
  if (state->active) {
    return MicDataStartErrBusy;
  }

  int16_t *buffer = applib_malloc(samples_per_update * sizeof(int16_t));
  if (!buffer) {
    return MicDataStartErrNoMemory;
  }

  const MicDataStartResult rv = (MicDataStartResult)sys_mic_capture_start(samples_per_update);
  if (rv != MicDataStartOk) {
    applib_free(buffer);
    return rv;
  }

  state->buffer = buffer;
  state->samples_per_update = samples_per_update;
  state->handlers = handlers;
  state->context = context;
  state->active = true;
  event_service_client_subscribe(&state->event_info);
  return MicDataStartOk;
}

void mic_data_service_unsubscribe(void) {
  MicDataServiceState *state = prv_get_state();
  if (!state || !state->active) {
    return;
  }
  sys_mic_capture_stop();
  prv_teardown(state);
}

bool mic_data_service_is_active(void) {
  MicDataServiceState *state = prv_get_state();
  return state ? state->active : false;
}

MicDataStartResult mic_stream_to_phone_start(MicStreamHandlers handlers, void *context) {
  MicDataServiceState *state = prv_get_state();
  if (!state) {
    return MicDataStartErrNotForeground;
  }
  if (!handlers.stopped) {
    return MicDataStartErrInvalidArgs;
  }
  if (state->active) {
    return MicDataStartErrBusy;
  }
  const MicDataStartResult rv = (MicDataStartResult)sys_mic_capture_start_stream();
  if (rv != MicDataStartOk) {
    return rv;
  }
  state->stream_handlers = handlers;
  state->context = context;
  state->active = true;
  state->streaming = true;
  event_service_client_subscribe(&state->event_info);
  return MicDataStartOk;
}

void mic_stream_to_phone_stop(void) {
  MicDataServiceState *state = prv_get_state();
  if (!state || !state->active || !state->streaming) {
    return;
  }
  sys_mic_capture_stop();
  prv_teardown(state);
}

bool mic_stream_to_phone_is_active(void) {
  MicDataServiceState *state = prv_get_state();
  return state ? (state->active && state->streaming) : false;
}

void mic_data_service_state_init(MicDataServiceState *state) {
  *state = (MicDataServiceState){
    .event_info = {
      .type = PEBBLE_MIC_CAPTURE_EVENT,
      .handler = prv_handle_event,
      .context = state,
    },
  };
}
