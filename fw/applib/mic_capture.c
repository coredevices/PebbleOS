/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/mic_capture.h>

#include <applib/mic_capture_private.h>
#include <kernel/events.h>
#include <kernel/pebble_tasks.h>
#include <process_state/app_state/app_state.h>
#include <syscall/syscall.h>

static MicCaptureState *prv_state(void) {
  return pebble_task_get_current() == PebbleTask_App ? app_state_get_mic_capture_state() : NULL;
}

static void prv_teardown(MicCaptureState *state) {
  event_service_client_unsubscribe(&state->event_info);
  state->session = 0;
  state->handlers = (MicCaptureHandlers){};
  state->context = NULL;
}

static void prv_event(PebbleEvent *event, void *context) {
  MicCaptureState *state = context;
  if (!state->session || state->session != event->mic_capture.session) {
    return;
  }
  if (event->mic_capture.type == MicCaptureEventData) {
    int16_t samples[MIC_CAPTURE_FRAME_SAMPLES];
    const size_t count = sys_mic_capture_read(state->session, samples);
    if (count) {
      state->handlers.data(samples, count, state->context);
    }
    return;
  }
  const MicCaptureStoppedHandler stopped = state->handlers.stopped;
  void *ctx = state->context;
  prv_teardown(state);
  stopped((MicCaptureStopReason)event->mic_capture.stop_reason, ctx);
}

MicCaptureStartResult mic_capture_start(MicCaptureHandlers handlers, void *context) {
  MicCaptureState *state = prv_state();
  if (!state)
    return MicCaptureStartErrNotForeground;
  if (!handlers.data || !handlers.stopped)
    return MicCaptureStartErrInvalidArgs;
  if (state->session)
    return MicCaptureStartErrBusy;
  state->handlers = handlers;
  state->context = context;
  // Subscribe before starting hardware so the first frame cannot be lost.
  event_service_client_subscribe(&state->event_info);
  const int32_t result = sys_mic_capture_start();
  if (result <= 0) {
    prv_teardown(state);
    return (MicCaptureStartResult)-result;
  }
  state->session = (uint32_t)result;
  return MicCaptureStartOk;
}

void mic_capture_stop(void) {
  MicCaptureState *state = prv_state();
  if (!state || !state->session)
    return;
  sys_mic_capture_stop(state->session);
  prv_teardown(state);
}

bool mic_capture_is_active(void) {
  MicCaptureState *state = prv_state();
  return state && state->session && sys_mic_capture_is_active(state->session);
}

void mic_capture_state_init(MicCaptureState *state) {
  *state = (MicCaptureState){
    .event_info = {.type = PEBBLE_MIC_CAPTURE_EVENT, .handler = prv_event, .context = state},
  };
}
