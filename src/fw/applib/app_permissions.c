/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_permissions.h"
#include "applib/app_permissions_private.h"

#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"

static AppPermissionServiceState *prv_get_state(void) {
  // Workers have no permissions of their own.
  if (pebble_task_get_current() != PebbleTask_App) {
    return NULL;
  }
  return app_state_get_app_permission_service_state();
}

static void prv_handle_event(PebbleEvent *e, void *context) {
  AppPermissionServiceState *state = context;
  if (state->handler) {
    state->handler((AppPermission)e->app_permission.permission,
                   (AppPermissionState)e->app_permission.state, state->context);
  }
}

AppPermissionState app_permission_get_state(AppPermission permission) {
  if ((permission >= AppPermissionCount) || (pebble_task_get_current() != PebbleTask_App)) {
    return AppPermissionStateNotDeclared;
  }
  return (AppPermissionState)sys_app_permission_get_state((uint8_t)permission);
}

bool app_permission_is_granted(AppPermission permission) {
  return (app_permission_get_state(permission) == AppPermissionStateGranted);
}

void app_permission_service_subscribe(AppPermissionChangedHandler handler, void *context) {
  AppPermissionServiceState *state = prv_get_state();
  if (!state) {
    return;
  }
  state->handler = handler;
  state->context = context;
  event_service_client_subscribe(&state->event_info);
}

void app_permission_service_unsubscribe(void) {
  AppPermissionServiceState *state = prv_get_state();
  if (!state) {
    return;
  }
  event_service_client_unsubscribe(&state->event_info);
  state->handler = NULL;
  state->context = NULL;
}

void app_permission_service_state_init(AppPermissionServiceState *state) {
  *state = (AppPermissionServiceState){
    .event_info = {
      .type = PEBBLE_APP_PERMISSION_EVENT,
      .handler = prv_handle_event,
      .context = state,
    },
  };
}
