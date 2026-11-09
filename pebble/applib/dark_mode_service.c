/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dark_mode_service.h"

#include "dark_mode_service_private.h"
#include "event_service_client.h"

#include <kernel/events.h>
#include <process_state/app_state/app_state.h>
#include <syscall/syscall.h>

static void prv_handle_dark_mode_change(PebbleEvent *e, void *context) {
  DarkModeServiceState *state = app_state_get_dark_mode_service_state();
  if (state->handler) {
    state->handler(sys_dark_mode_is_active());
  }
}

void dark_mode_service_subscribe(DarkModeHandler handler) {
  dark_mode_service_unsubscribe();
  if (!handler) {
    return;
  }
  DarkModeServiceState *state = app_state_get_dark_mode_service_state();
  state->handler = handler;
  event_service_client_subscribe(&state->dark_mode_info);
}

void dark_mode_service_unsubscribe(void) {
  DarkModeServiceState *state = app_state_get_dark_mode_service_state();
  if (state->handler) {
    event_service_client_unsubscribe(&state->dark_mode_info);
    state->handler = nullptr;
  }
}

bool dark_mode_service_peek(void) {
  return sys_dark_mode_is_active();
}

void dark_mode_service_state_init(DarkModeServiceState *state) {
  *state = (DarkModeServiceState){
    .dark_mode_info = {
      .type = PEBBLE_DARK_MODE_CHANGE_EVENT,
      .handler = prv_handle_dark_mode_change,
    },
  };
}
