/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/mic_stream.h"
#include "applib/mic_stream_private.h"

#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/mic_stream.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"

_Static_assert((int)MicStreamStartErrUnavailable == (int)MicStreamServiceStartErrUnavailable,
               "MicStreamStartResult must match the kernel");
_Static_assert((int)MicStreamStopReasonError == (int)MicStreamServiceStopReasonError,
               "MicStreamStopReason must match the kernel");

static MicStreamState *prv_get_state(void) {
  if (pebble_task_get_current() != PebbleTask_App) {
    return NULL;
  }
  return app_state_get_mic_stream_state();
}

static void prv_teardown(MicStreamState *state) {
  event_service_client_unsubscribe(&state->event_info);
  state->handlers = (MicStreamHandlers){};
  state->context = NULL;
  state->active = false;
}

static void prv_handle_event(PebbleEvent *e, void *context) {
  MicStreamState *state = context;
  if (!state->active) {
    return;
  }
  if (e->mic_stream.type == MicStreamEventStarted) {
    if (state->handlers.started) {
      state->handlers.started(state->context);
    }
    return;
  }
  const MicStreamStoppedHandler stopped = state->handlers.stopped;
  void *ctx = state->context;
  prv_teardown(state);
  stopped((MicStreamStopReason)e->mic_stream.stop_reason, ctx);
}

MicStreamStartResult mic_stream_to_phone_start(MicStreamHandlers handlers, void *context) {
  MicStreamState *state = prv_get_state();
  if (!state) {
    return MicStreamStartErrNotForeground;
  }
  if (!handlers.stopped) {
    return MicStreamStartErrInvalidArgs;
  }
  if (state->active) {
    return MicStreamStartErrBusy;
  }
  const MicStreamStartResult rv = (MicStreamStartResult)sys_mic_stream_start();
  if (rv != MicStreamStartOk) {
    return rv;
  }
  state->handlers = handlers;
  state->context = context;
  state->active = true;
  event_service_client_subscribe(&state->event_info);
  return MicStreamStartOk;
}

void mic_stream_to_phone_stop(void) {
  MicStreamState *state = prv_get_state();
  if (!state || !state->active) {
    return;
  }
  sys_mic_stream_stop();
  prv_teardown(state);
}

bool mic_stream_to_phone_is_active(void) {
  MicStreamState *state = prv_get_state();
  return state ? state->active : false;
}

void mic_stream_state_init(MicStreamState *state) {
  *state = (MicStreamState){
    .event_info = {
      .type = PEBBLE_MIC_STREAM_EVENT,
      .handler = prv_handle_event,
      .context = state,
    },
  };
}
