/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/dark_mode_service.h>
#include <applib/dark_mode_service_private.h>
#include <clar.h>
#include <kernel/events.h>

// Fake event service
//////////////////////////////////////////
static int s_subscribe_count;
static int s_unsubscribe_count;
static EventServiceInfo *s_subscribed_info;

void event_service_client_subscribe(EventServiceInfo *info) {
  s_subscribe_count++;
  s_subscribed_info = info;
}

void event_service_client_unsubscribe(EventServiceInfo *info) {
  s_unsubscribe_count++;
  cl_assert_equal_p(info, s_subscribed_info);
  s_subscribed_info = nullptr;
}

// State and syscall
//////////////////////////////////////////
static DarkModeServiceState s_state;
static bool s_is_dark;

DarkModeServiceState *app_state_get_dark_mode_service_state(void) {
  return &s_state;
}

bool sys_dark_mode_is_active(void) {
  return s_is_dark;
}

// Handler bookkeeping
//////////////////////////////////////////
static int s_calls;
static bool s_last_is_dark;

static void prv_handler(bool is_dark) {
  s_calls++;
  s_last_is_dark = is_dark;
}

static void prv_put_event(void) {
  cl_assert(s_subscribed_info);
  PebbleEvent event = {.type = PEBBLE_DARK_MODE_CHANGE_EVENT};
  s_subscribed_info->handler(&event, s_subscribed_info->context);
}

void test_dark_mode_service__initialize(void) {
  s_subscribe_count = 0;
  s_unsubscribe_count = 0;
  s_subscribed_info = nullptr;
  s_is_dark = false;
  s_calls = 0;
  s_last_is_dark = false;
  dark_mode_service_state_init(&s_state);
}

void test_dark_mode_service__peek(void) {
  cl_assert(!dark_mode_service_peek());
  s_is_dark = true;
  cl_assert(dark_mode_service_peek());
}

void test_dark_mode_service__subscribe_calls_handler_with_the_new_state(void) {
  dark_mode_service_subscribe(prv_handler);
  cl_assert_equal_i(s_subscribe_count, 1);
  cl_assert_equal_i(s_subscribed_info->type, PEBBLE_DARK_MODE_CHANGE_EVENT);

  s_is_dark = true;
  prv_put_event();
  cl_assert_equal_i(s_calls, 1);
  cl_assert(s_last_is_dark);

  s_is_dark = false;
  prv_put_event();
  cl_assert_equal_i(s_calls, 2);
  cl_assert(!s_last_is_dark);
}

void test_dark_mode_service__unsubscribe(void) {
  dark_mode_service_subscribe(prv_handler);
  dark_mode_service_unsubscribe();
  cl_assert_equal_i(s_unsubscribe_count, 1);
  cl_assert_equal_p(s_subscribed_info, nullptr);
  cl_assert_equal_p(s_state.handler, nullptr);

  // Unsubscribing again does nothing
  dark_mode_service_unsubscribe();
  cl_assert_equal_i(s_unsubscribe_count, 1);
}

void test_dark_mode_service__subscribe_twice_replaces_the_handler(void) {
  dark_mode_service_subscribe(prv_handler);
  dark_mode_service_subscribe(prv_handler);
  cl_assert_equal_i(s_subscribe_count, 2);
  cl_assert_equal_i(s_unsubscribe_count, 1);

  s_is_dark = true;
  prv_put_event();
  cl_assert_equal_i(s_calls, 1);
}

void test_dark_mode_service__subscribe_null_unsubscribes(void) {
  dark_mode_service_subscribe(prv_handler);
  dark_mode_service_subscribe(nullptr);
  cl_assert_equal_i(s_unsubscribe_count, 1);
  cl_assert_equal_p(s_subscribed_info, nullptr);
}
