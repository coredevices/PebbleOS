/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "clar_asserts.h"

#include "applib/event_service_client.h"
#include "fake_events.h"

static EventServiceInfo s_event_handler[PEBBLE_NUM_EVENTS];

void event_service_client_subscribe(EventServiceInfo *service_info) {
  cl_assert_equal_p(s_event_handler[service_info->type].handler, NULL);
  s_event_handler[service_info->type] = *service_info;
}

void event_service_client_unsubscribe(EventServiceInfo *service_info) {
  s_event_handler[service_info->type] = (EventServiceInfo) {};
}

void fake_event_service_init(void) {
  memset(s_event_handler, sizeof(s_event_handler), 0);
}

void fake_event_service_handle_last(void) {
  PebbleEvent event = fake_event_get_last();
  EventServiceInfo *service_info = &s_event_handler[event.type];
  cl_assert(service_info->handler);
  service_info->handler(&event, service_info->context);
}

EventServiceInfo *fake_event_service_get_info(PebbleEventType type) {
  return &s_event_handler[type];
}

