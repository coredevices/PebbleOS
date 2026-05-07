/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "backlight_service.h"
#include "backlight_service_private.h"

#include "event_service_client.h"
#include "kernel/events.h"
#include "process_state/app_state/app_state.h"
#include "process_state/worker_state/worker_state.h"
#include "system/passert.h"

static BacklightServiceState *prv_get_state(void) {
  PebbleTask task = pebble_task_get_current();

  if (task == PebbleTask_App) {
    return app_state_get_backlight_service_state();
  } else if (task == PebbleTask_Worker) {
    return worker_state_get_backlight_service_state();
  }

  WTF;
}

static void prv_do_handle(PebbleEvent *e, void *context) {
  BacklightServiceState *state = prv_get_state();
  if (state->handler != NULL) {
    state->handler(e->backlight.is_on);
  }
}

void backlight_service_subscribe(BacklightHandler handler) {
  BacklightServiceState *state = prv_get_state();
  state->handler = handler;
  event_service_client_subscribe(&state->bls_info);
}

void backlight_service_unsubscribe(void) {
  BacklightServiceState *state = prv_get_state();
  event_service_client_unsubscribe(&state->bls_info);
  state->handler = NULL;
}

void backlight_service_state_init(BacklightServiceState *state) {
  *state = (BacklightServiceState) {
    .bls_info = {
      .type = PEBBLE_BACKLIGHT_EVENT,
      .handler = prv_do_handle,
    },
  };
}
