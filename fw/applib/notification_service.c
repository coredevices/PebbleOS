/* SPDX-FileCopyrightText: 2026 Eyal Leifenberg */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notification_service.h"

#include "event_service_client.h"
#include "notification_service_private.h"

#include <kernel/events.h>
#include <process_state/app_state/app_state.h>
#include <syscall/syscall.h>

static NotificationServiceState *prv_get_state(void) {
  return app_state_get_notification_service_state();
}

static void prv_handle_notification_event(PebbleEvent *event, void *context) {
  NotificationServiceState *state = context;
  if ((event->sys_notification.type != NotificationAdded) &&
      (event->sys_notification.type != NotificationRemoved)) {
    return;
  }

  if (state->handler) {
    state->handler(notification_service_peek_count());
  }
}

void notification_service_subscribe(NotificationCountHandler handler) {
  NotificationServiceState *state = prv_get_state();
  state->handler = handler;
  event_service_client_subscribe(&state->event_info);
}

void notification_service_unsubscribe(void) {
  NotificationServiceState *state = prv_get_state();
  event_service_client_unsubscribe(&state->event_info);
  state->handler = nullptr;
}

uint16_t notification_service_peek_count(void) {
  return sys_notification_service_get_count();
}

void notification_service_state_init(NotificationServiceState *state) {
  *state = (NotificationServiceState){
    .event_info = {
      .type = PEBBLE_SYS_NOTIFICATION_EVENT,
      .handler = prv_handle_notification_event,
      .context = state,
    },
  };
}
