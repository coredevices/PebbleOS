/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch_service.h"
#include "touch_service_private.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "kernel/kernel_applib_state.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/touch/touch.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"
#include "system/passert.h"

bool sys_touch_service_is_enabled(void);

//! @return the per-task touch service state, or NULL if the current task is
//! not permitted to use the touch service (e.g. background workers). Callers
//! must no-op when this returns NULL.
static TouchServiceState *prv_get_state(void) {
  PebbleTask task = pebble_task_get_current();
  switch (task) {
    case PebbleTask_App:
      return app_state_get_touch_service_state();
    case PebbleTask_KernelMain:
      return kernel_applib_get_touch_service_state();
    case PebbleTask_Worker:
      // Touch is not available to background workers — they have no UI.
      return NULL;
    default:
      WTF;
  }
}

static void prv_handle_touch_event(PebbleEvent *e, void *context) {
  TouchServiceState *state = prv_get_state();
  if (state->raw_handler && e->type == PEBBLE_TOUCH_EVENT) {
    state->raw_handler(&e->touch.event, state->raw_context);
  }
}

void touch_service_subscribe(TouchServiceHandler handler, void *context) {
  TouchServiceState *state = prv_get_state();
  if (!state) {
    return;
  }
  state->raw_handler = handler;
  state->raw_context = context;

  state->raw_event_info = (EventServiceInfo) {
    .type = PEBBLE_TOUCH_EVENT,
    .handler = prv_handle_touch_event,
  };
  touch_reset();
  if (!state->raw_subscribed) {
    event_service_client_subscribe(&state->raw_event_info);
    state->raw_subscribed = true;
  }
}

void touch_service_unsubscribe(void) {
  TouchServiceState *state = prv_get_state();
  if (!state) {
    return;
  }
  if (state->raw_subscribed) {
    event_service_client_unsubscribe(&state->raw_event_info);
    state->raw_subscribed = false;
  }
  state->raw_handler = NULL;
  state->raw_context = NULL;
}

bool touch_service_is_enabled(void) {
  return sys_touch_service_is_enabled();
}

void touch_service_state_init(TouchServiceState *state) {
  *state = (TouchServiceState){ 0 };
}
