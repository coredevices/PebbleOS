/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch_service.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "pbl/services/common/touch/touch.h"

static TouchServiceHandler s_handler;
static void *s_context;

static void prv_handle_touch_event(PebbleEvent *e, void *context) {
  if (s_handler && e->type == PEBBLE_TOUCH_EVENT) {
    s_handler(&e->touch.event, s_context);
  }
}

static EventServiceInfo s_event_info;

void touch_service_subscribe(TouchServiceHandler handler, void *context) {
  s_handler = handler;
  s_context = context;

  s_event_info = (EventServiceInfo) {
    .type = PEBBLE_TOUCH_EVENT,
    .handler = prv_handle_touch_event,
  };
  touch_reset();
  event_service_client_subscribe(&s_event_info);
}

void touch_service_unsubscribe(void) {
  event_service_client_unsubscribe(&s_event_info);
  s_handler = NULL;
  s_context = NULL;
}
