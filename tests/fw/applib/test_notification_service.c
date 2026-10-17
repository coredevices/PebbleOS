/* SPDX-FileCopyrightText: 2026 Eyal Leifenberg */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/notification_service.h>
#include <applib/notification_service_private.h>
#include <clar.h>
#include <kernel/events.h>

static NotificationServiceState s_state;
static EventServiceInfo *s_subscribed_info;
static uint16_t s_count;
static uint16_t s_handler_count;
static unsigned int s_handler_calls;

NotificationServiceState *app_state_get_notification_service_state(void) {
  return &s_state;
}

void event_service_client_subscribe(EventServiceInfo *info) {
  s_subscribed_info = info;
}

void event_service_client_unsubscribe(EventServiceInfo *info) {
  cl_assert_equal_p(info, s_subscribed_info);
  s_subscribed_info = nullptr;
}

uint16_t sys_notification_service_get_count(void) {
  return s_count;
}

static void prv_handler(uint16_t count) {
  s_handler_count = count;
  s_handler_calls++;
}

static void prv_deliver(PebbleSysNotificationType type) {
  cl_assert(s_subscribed_info);
  PebbleEvent event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification.type = type,
  };
  s_subscribed_info->handler(&event, s_subscribed_info->context);
}

void test_notification_service__initialize(void) {
  s_state = (NotificationServiceState){};
  s_subscribed_info = nullptr;
  s_count = 0;
  s_handler_count = 0;
  s_handler_calls = 0;
  notification_service_state_init(&s_state);
}

void test_notification_service__cleanup(void) {
}

void test_notification_service__peek_returns_current_count(void) {
  s_count = 7;
  cl_assert_equal_i(notification_service_peek_count(), 7);
}

void test_notification_service__notifies_after_add_and_remove(void) {
  notification_service_subscribe(prv_handler);

  s_count = 3;
  prv_deliver(NotificationAdded);
  cl_assert_equal_i(s_handler_calls, 1);
  cl_assert_equal_i(s_handler_count, 3);

  s_count = 2;
  prv_deliver(NotificationRemoved);
  cl_assert_equal_i(s_handler_calls, 2);
  cl_assert_equal_i(s_handler_count, 2);
}

void test_notification_service__ignores_events_that_do_not_change_count(void) {
  notification_service_subscribe(prv_handler);

  prv_deliver(NotificationActedUpon);
  prv_deliver(NotificationActionResult);
  cl_assert_equal_i(s_handler_calls, 0);
}

void test_notification_service__unsubscribe_stops_delivery(void) {
  notification_service_subscribe(prv_handler);
  notification_service_unsubscribe();

  cl_assert_equal_p(s_subscribed_info, nullptr);
  cl_assert_equal_p(s_state.handler, nullptr);
}
