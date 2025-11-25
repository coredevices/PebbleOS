/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/debounced_connection_service.h"
#include "services/common/regular_timer.h"
#include "syscall/syscall.h"

#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_system_task.h"
#include "fake_pbl_malloc.h"
#include "fake_session.h"
#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_hexdump.h"
#include "stubs_passert.h"

// Fakes
///////////////////////////////////////////////////////////

static TimerID s_debounce_timer;
static bool s_default_connection_state;
static unsigned int s_event_count = 0;
static PebbleEvent s_cached_event;

static Transport *s_transport;
static CommSession *s_session;

bool remote_is_connected(void) {
  return s_default_connection_state;
}

void event_put(PebbleEvent* event) {
  s_event_count++;

  s_cached_event = (PebbleEvent) {
    .type = event->type,
    .bluetooth.comm_session_event = event->bluetooth.comm_session_event
  };
}

// Helper functions
///////////////////////////////////////////////////////////
static void init(bool connected) {
  if (connected) {
    s_transport = fake_transport_create(TransportDestinationSystem, NULL, NULL);
    s_session = fake_transport_set_connected(s_transport, true);
  }
  
  s_default_connection_state = connected;

  regular_timer_init();
  debounced_connection_service_init();
}

static void check_connected(void) {
  // Connected = true
  cl_assert(sys_mobile_app_is_connected_debounced());
}

static void check_waiting_to_send_disconnect(void) {
  // Connected = true
  cl_assert(sys_mobile_app_is_connected_debounced());
}

static void check_waiting_to_send_second_disconnect(void) {
  // Connected = false
  cl_assert(!sys_mobile_app_is_connected_debounced());
}

static void check_disconnected(void) {
  // Connected = false
  cl_assert(!sys_mobile_app_is_connected_debounced());
}

static void prv_send_connection_event(bool is_connected) {
  //! Get connected event
  PebbleCommSessionEvent event = {
    .is_open = is_connected,
    .is_system = true,
  };
  debounced_connection_service_handle_event(&event);
}

static void prv_assert_event_received(bool is_connected) {
  cl_assert_equal_i(s_event_count, 1);
  cl_assert_equal_b(s_cached_event.bluetooth.comm_session_event.is_open, is_connected);
}

// Tests
///////////////////////////////////////////////////////////

void test_debounced_connection_service__cleanup(void) {
  s_event_count = 0;
  regular_timer_deinit();
  fake_comm_session_cleanup();
}

void test_debounced_connection_service__connected_to_disconnected(void) {
  init(true /* connected */);

  check_connected();

  //! Get disconnected event
  prv_send_connection_event(false);

  check_waiting_to_send_disconnect();

  // No event put
  cl_assert_equal_i(s_event_count, 0);

  //! Timer fires
  regular_timer_fire_seconds(1);

  // Event put
  prv_assert_event_received(false);

  check_disconnected();
}

void test_debounced_connection_service__disconnected_to_connected(void) {
  init(false /* disconnected */);

  check_disconnected();

  //! Get connected event
  prv_send_connection_event(true);

  // Event put
  prv_assert_event_received(true);

  check_connected();
}

void test_debounced_connection_service__connected_to_connected(void) {
  init(true /* connected */);

  check_connected();

  //! Get connected event
  prv_send_connection_event(true);

  check_connected();

  // Event put
  prv_assert_event_received(true);
}

void test_debounced_connection_service__disconnected_to_disconnected(void) {
  //! Currently disconnected
  init(false /* disconnected */);

  check_disconnected();

  //! Get disconnected event
  prv_send_connection_event(false);

  check_waiting_to_send_second_disconnect();

  // No event put
  cl_assert_equal_i(s_event_count, 0);
}

void test_debounced_connection_service__disconnected_wait_disconnected(void) {
  //! Currently disconnected
  init(false /* disconnected */);

  check_disconnected();

  //! Get disconnected event
  prv_send_connection_event(false);

  check_waiting_to_send_second_disconnect();

  //! Timer fires
  regular_timer_fire_seconds(1);

  check_disconnected();

  // Event put
  prv_assert_event_received(false);
}

void test_debounced_connection_service__reconnected_quickly(void) {
  //! Currently connected
  init(true /* connected */);

  check_connected();

  //! Get disconnected event
  prv_send_connection_event(false);
  check_waiting_to_send_disconnect();

  // No event put
  cl_assert_equal_i(s_event_count, 0);

  //! Get connected event before timer fires
  prv_send_connection_event(true);

  check_connected();

  // No event put
  cl_assert_equal_i(s_event_count, 0);
}
