/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/bluetooth/ble_hrm.h"

#include "comm/ble/gap_le_connection.h"
#include "pbl/services/hrm/hrm_manager_private.h"

#include <bluetooth/hrm_service.h>
#include <btutil/bt_device.h>
#include <util/size.h>

#include <clar.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// Stubs & Fakes

#include "fake_event_service.h"
#include "fake_pebble_tasks.h"
#include "fake_pbl_malloc.h"
#include "fake_regular_timer.h"

#include "stubs_analytics.h"
#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

static bool s_activity_prefs_heart_rate_is_enabled;
bool activity_prefs_heart_rate_is_enabled(void) {
  return s_activity_prefs_heart_rate_is_enabled;
}

static bool s_bt_driver_hrm_service_is_enabled;
static int s_bt_driver_hrm_service_enable_call_count;
void bt_driver_hrm_service_enable(bool enable) {
  s_bt_driver_hrm_service_enable_call_count++;
  s_bt_driver_hrm_service_is_enabled = enable;
}

static int s_bt_driver_hrm_service_handle_measurement_call_count;
void bt_driver_hrm_service_handle_measurement(const BleHrmServiceMeasurement *measurement,
                                              const BTDeviceInternal *permitted_devices,
                                              size_t num_permitted_devices) {
  ++s_bt_driver_hrm_service_handle_measurement_call_count;
}

bool bt_driver_is_hrm_service_supported(void) {
  return true;
}

int bt_driver_gap_le_disconnect(const BTDeviceInternal *peer_address) {
  return 0;
}

static int s_hrm_manager_subscribe_with_callback_call_count;
static HRMSessionRef s_last_session_ref;
static HRMSessionRef s_next_session_ref;
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  HRMSubscriberCallback callback, void *context) {
  cl_assert_equal_p(NULL, callback); // we're using the event service
  cl_assert_equal_i(features, HRMFeature_BPM);
  ++s_hrm_manager_subscribe_with_callback_call_count;
  s_last_session_ref = ++s_next_session_ref;
  return s_last_session_ref;
}

static GAPLEConnection *s_connections[2];

GAPLEConnection *gap_le_connection_by_device(const BTDeviceInternal *device) {
  for (int i = 0; i < ARRAY_LENGTH(s_connections); ++i) {
    if (bt_device_internal_equal(device, &s_connections[i]->device)) {
      return s_connections[i];
    }
  }
  return NULL;
}
BTDeviceInternal *device_from_le_connection(GAPLEConnection *conn) {
  return &conn->device;
}

bool gap_le_connection_is_valid(const GAPLEConnection *conn) {
  for (int i = 0; i < ARRAY_LENGTH(s_connections); ++i) {
    if (s_connections[i] == conn) {
      return true;
    }
  }
  return false;
}

void gap_le_connection_for_each(GAPLEConnectionForEachCallback cb, void *data) {
  for (int i = 0; i < ARRAY_LENGTH(s_connections); ++i) {
    cb(s_connections[i], data);
  }
}

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  callback(data);
}

bool sys_hrm_manager_is_hrm_present(void) {
  return true;
}

static int s_sys_hrm_manager_unsubscribe_call_count;
bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  ++s_sys_hrm_manager_unsubscribe_call_count;
  cl_assert_equal_i(session, s_last_session_ref);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

static void prv_assert_event_service_subscribed(bool is_subscribed) {
  const EventServiceInfo *const info = fake_event_service_get_info(PEBBLE_HRM_EVENT);
  if (is_subscribed) {
    cl_assert(info->handler);
  } else {
    cl_assert_equal_p(NULL, info->handler);
  }
}

void test_ble_hrm__cleanup(void) {
  ble_hrm_deinit();

  prv_assert_event_service_subscribed(false);
  cl_assert_equal_i(s_sys_hrm_manager_unsubscribe_call_count,
                    s_hrm_manager_subscribe_with_callback_call_count);

  fake_pbl_malloc_check_net_allocs();

  // Assert all regular timers are deregistered:
  cl_assert_equal_p(s_seconds_callbacks.next, NULL);
  cl_assert_equal_p(s_minutes_callbacks.next, NULL);
}

#define TEST_DEVICE_NAME "iPhone Martijn"

static GAPLEConnection s_conn_a;
static const BTDeviceInternal *s_device_a;

void test_ble_hrm__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  for (int i = 0; i < ARRAY_LENGTH(s_connections); ++i) {
    s_connections[i] = NULL;
  }
  s_activity_prefs_heart_rate_is_enabled = true;
  s_bt_driver_hrm_service_is_enabled = true;
  s_bt_driver_hrm_service_enable_call_count = 0;
  s_hrm_manager_subscribe_with_callback_call_count = 0;
  s_sys_hrm_manager_unsubscribe_call_count = 0;
  s_bt_driver_hrm_service_handle_measurement_call_count = 0;
  s_last_session_ref = ~0;
  s_next_session_ref = 1234;
  fake_event_service_init();

  s_conn_a = (GAPLEConnection) {
    .device_name = TEST_DEVICE_NAME,
    .device = {
      .address = {
        .octets = {1, 2, 3, 4, 5, 6},
      },
    },
  };
  s_connections[0] = &s_conn_a;
  s_device_a = device_from_le_connection(&s_conn_a);

  ble_hrm_init();
}

void test_ble_hrm__init_deinit_no_subscriptions(void) {
  // let cleanup & initialize do the work :)
}

// The HRM GATT service is exposed, but the watch does not share heart rate yet
// (that arrives, gated to workouts, in a later change). A subscriber receives
// nothing and is never prompted.
void test_ble_hrm__no_sharing_or_prompt(void) {
  cl_assert_equal_i(s_hrm_manager_subscribe_with_callback_call_count, 0);
  prv_assert_event_service_subscribed(false);

  bt_driver_cb_hrm_service_update_subscription(s_device_a, true);

  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));
  cl_assert_equal_b(false, ble_hrm_is_sharing());
  cl_assert_equal_i(s_hrm_manager_subscribe_with_callback_call_count, 0);
  prv_assert_event_service_subscribed(false);

  bt_driver_cb_hrm_service_update_subscription(s_device_a, false);
}

// Handle a subscription/disconnection callback that arrives after deinit:
void test_ble_hrm__sub_after_deinit(void) {
  ble_hrm_deinit();

  bt_driver_cb_hrm_service_update_subscription(s_device_a, true);
  prv_assert_event_service_subscribed(false);
  cl_assert_equal_i(s_hrm_manager_subscribe_with_callback_call_count, 0);

  ble_hrm_handle_disconnection(&s_conn_a);
  prv_assert_event_service_subscribed(false);

  ble_hrm_init(); // reinit, __cleanup() will deinit again
}

void test_ble_hrm__handle_activity_pref_hrm_changes(void) {
  cl_assert_equal_b(true, s_bt_driver_hrm_service_is_enabled);
  cl_assert_equal_i(0, s_bt_driver_hrm_service_enable_call_count);
  s_activity_prefs_heart_rate_is_enabled = false;
  ble_hrm_handle_activity_prefs_heart_rate_is_enabled(false);
  cl_assert_equal_i(1, s_bt_driver_hrm_service_enable_call_count);
  cl_assert_equal_b(false, s_bt_driver_hrm_service_is_enabled);

  ble_hrm_handle_activity_prefs_heart_rate_is_enabled(false);
  cl_assert_equal_i(2, s_bt_driver_hrm_service_enable_call_count);
  cl_assert_equal_b(false, s_bt_driver_hrm_service_is_enabled);

  ble_hrm_handle_activity_prefs_heart_rate_is_enabled(true);
  cl_assert_equal_i(3, s_bt_driver_hrm_service_enable_call_count);
  cl_assert_equal_b(true, s_bt_driver_hrm_service_is_enabled);
}
