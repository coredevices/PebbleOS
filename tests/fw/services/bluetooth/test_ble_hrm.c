/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/hrm_service.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/services/bluetooth/ble_hrm.h>
#include <pbl/services/hrm/hrm_manager_private.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <comm/ble/gap_le_connection.h>
#include <fff.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// Stubs & Fakes

#include <fake_event_service.h>
#include <fake_pbl_malloc.h>
#include <fake_pebble_tasks.h>
#include <fake_regular_timer.h>

FAKE_VOID_FUNC(gap_le_slave_reconnect_hrm_restart);
FAKE_VOID_FUNC(gap_le_slave_reconnect_hrm_stop);
FAKE_VALUE_FUNC(bool, activity_prefs_heart_rate_is_enabled);
FAKE_VOID_FUNC(pbl_bt_hrm_service_enable, bool);
FAKE_VOID_FUNC(pbl_bt_hrm_service_handle_measurement, const struct pbl_bt_hrm_service_measurement *,
               const struct pbl_bt_device_internal *, size_t);
FAKE_VOID_FUNC(ble_hrm_push_sharing_request_window, BLEHRMSharingRequest *);
FAKE_VALUE_FUNC(int, pbl_bt_gap_le_disconnect, const struct pbl_bt_device_internal *);
FAKE_VOID_FUNC(ble_hrm_push_reminder_popup);
FAKE_VALUE_FUNC(HRMSessionRef, hrm_manager_subscribe_with_callback, AppInstallId, uint32_t,
                uint16_t, HRMFeature, bool, HRMSubscriberCallback, void *);
FAKE_VALUE_FUNC(bool, sys_hrm_manager_unsubscribe, HRMSessionRef);

#define FAKES(FAKE)                           \
  FAKE(gap_le_slave_reconnect_hrm_restart)    \
  FAKE(gap_le_slave_reconnect_hrm_stop)       \
  FAKE(activity_prefs_heart_rate_is_enabled)  \
  FAKE(pbl_bt_hrm_service_enable)             \
  FAKE(pbl_bt_hrm_service_handle_measurement) \
  FAKE(ble_hrm_push_sharing_request_window)   \
  FAKE(pbl_bt_gap_le_disconnect)              \
  FAKE(ble_hrm_push_reminder_popup)           \
  FAKE(hrm_manager_subscribe_with_callback)   \
  FAKE(sys_hrm_manager_unsubscribe)

// The measurement and device list live on the caller's stack.
static struct pbl_bt_hrm_service_measurement s_last_ble_hrm_measurement;
static struct pbl_bt_device_internal s_last_permitted_devices[10];
static void prv_handle_measurement(const struct pbl_bt_hrm_service_measurement *measurement,
                                   const struct pbl_bt_device_internal *permitted_devices,
                                   size_t num_permitted_devices) {
  s_last_ble_hrm_measurement = *measurement;
  memcpy(s_last_permitted_devices, permitted_devices,
         sizeof(*permitted_devices) * num_permitted_devices);
}

static void prv_assert_last_disconnected(const struct pbl_bt_device_internal *peer_address) {
  cl_assert_equal_b(bt_device_internal_equal(peer_address, pbl_bt_gap_le_disconnect_fake.arg0_val),
                    true);
}

static GAPLEConnection *s_connections[2];

GAPLEConnection *gap_le_connection_by_device(const struct pbl_bt_device_internal *device) {
  for (int i = 0; i < ARRAY_LENGTH(s_connections); ++i) {
    if (bt_device_internal_equal(device, &s_connections[i]->device)) {
      return s_connections[i];
    }
  }
  return nullptr;
}
struct pbl_bt_device_internal *device_from_le_connection(GAPLEConnection *conn) {
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

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

static void prv_assert_event_service_subscribed(bool is_subscribed) {
  const EventServiceInfo *const info = fake_event_service_get_info(PEBBLE_HRM_EVENT);
  if (is_subscribed) {
    cl_assert(info->handler);
  } else {
    cl_assert_equal_p(nullptr, info->handler);
  }
}

void test_ble_hrm__cleanup(void) {
  ble_hrm_deinit();

  prv_assert_event_service_subscribed(false);
  // hrm manager sub vs unsub calls should be the same, there should be no subscription any more
  // after de-initing:
  cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.call_count,
                    hrm_manager_subscribe_with_callback_fake.call_count);
  for (unsigned int i = 0; i < hrm_manager_subscribe_with_callback_fake.call_count; ++i) {
    cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.arg3_history[i], HRMFeature_BPM);
    // the relay copes with batched FIFO drains; keep the cheap cadence
    cl_assert(!hrm_manager_subscribe_with_callback_fake.arg4_history[i]);
    // we're using the event service
    cl_assert_equal_p(hrm_manager_subscribe_with_callback_fake.arg5_history[i], nullptr);
    cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.arg0_history[i], 1234);
  }

  fake_pbl_malloc_check_net_allocs();

  // Assert all regular timers are deregistered:
  cl_assert_equal_p(s_seconds_callbacks.next, nullptr);
  cl_assert_equal_p(s_minutes_callbacks.next, nullptr);
}

#define TEST_DEVICE_NAME "iPhone Martijn"

static GAPLEConnection s_conn_a;
static GAPLEConnection s_conn_b;
static const struct pbl_bt_device_internal *s_device_a;
static const struct pbl_bt_device_internal *s_device_b;

void test_ble_hrm__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  for (int i = 0; i < ARRAY_LENGTH(s_connections); ++i) {
    s_connections[i] = nullptr;
  }
  FAKES(RESET_FAKE);
  FFF_RESET_HISTORY();
  activity_prefs_heart_rate_is_enabled_fake.return_val = true;
  pbl_bt_hrm_service_handle_measurement_fake.custom_fake = prv_handle_measurement;
  hrm_manager_subscribe_with_callback_fake.return_val = 1234;
  sys_hrm_manager_unsubscribe_fake.return_val = true;
  memset(s_last_permitted_devices, 0, sizeof(s_last_permitted_devices));
  s_last_ble_hrm_measurement = (struct pbl_bt_hrm_service_measurement){};
  fake_event_service_init();

  // Set up fake devices/connections:
  s_conn_a = (GAPLEConnection){
    .device_name = TEST_DEVICE_NAME,
    .device = {
      .address = {
        .octets = {1, 2, 3, 4, 5, 6},
      },
    },
  };
  s_conn_b = (GAPLEConnection){
    .device_name = TEST_DEVICE_NAME,
    .device = {
      .address = {
        .octets = {6, 5, 4, 3, 2, 1},
      },
    },
  };
  s_connections[0] = &s_conn_a;
  s_connections[1] = &s_conn_b;
  s_device_a = device_from_le_connection(&s_conn_a);
  s_device_b = device_from_le_connection(&s_conn_b);

  ble_hrm_init();
}

void test_ble_hrm__init_deinit_no_subscriptions(void) {
  // let cleanup & initialize do the work :)
}

static void prv_assert_permissions_ui_and_respond(bool is_granted) {
  cl_assert_equal_i(ble_hrm_push_sharing_request_window_fake.call_count, 1);
  ble_hrm_handle_sharing_request_response(is_granted,
                                          ble_hrm_push_sharing_request_window_fake.arg0_val);
  RESET_FAKE(ble_hrm_push_sharing_request_window);
}

void test_ble_hrm__sub_unsub(void) {
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 0);
  cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.call_count, 0);
  prv_assert_event_service_subscribed(false);

  // Device A subscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // Expect HRM manager NOT to be subscribed to yet, need to grant permission first:
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 0);
  prv_assert_event_service_subscribed(false);
  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));

  // Expect permissions UI to be presented:
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  // Expect HRM manager to be subscribed to:
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 1);
  prv_assert_event_service_subscribed(true);
  cl_assert_equal_b(true, ble_hrm_is_sharing_to_connection(&s_conn_a));

  // Device A subscribes again, should be a no-op, no new permissions prompt:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 1);
  cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.call_count, 0);

  // Device B subscribes, shouldn't resubscribe to HRM manager, but should present a new
  // permission prompt, because it's a different device:
  pbl_bt_cb_hrm_service_update_subscription(s_device_b, true);
  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_b));
  prv_assert_permissions_ui_and_respond(true /* is_granted */);
  cl_assert_equal_b(true, ble_hrm_is_sharing_to_connection(&s_conn_b));
  prv_assert_event_service_subscribed(true);
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 1);
  cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.call_count, 0);

  // Device A disconnects, shouldn't unsubscribe from HRM manager because A is still subscribed:
  ble_hrm_handle_disconnection(&s_conn_a);
  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));
  prv_assert_event_service_subscribed(true);
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 1);
  cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.call_count, 0);

  // Device B unsubscribes, expect to be unsubscribed from HRM manager, because there are no more
  // devices subscribed to the BLE HRM service:
  pbl_bt_cb_hrm_service_update_subscription(s_device_b, false);
  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));
  prv_assert_event_service_subscribed(false);
  cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.call_count, 1);

  // Device B unsubscribes again, should be no-op
  pbl_bt_cb_hrm_service_update_subscription(s_device_b, false);
  prv_assert_event_service_subscribed(false);
  cl_assert_equal_i(sys_hrm_manager_unsubscribe_fake.call_count, 1);
}

void test_ble_hrm__sub_unsub_resub(void) {
  // Device A subscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // Expect permissions UI to be presented:
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  // Device A unsubscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, false);

  prv_assert_event_service_subscribed(false);

  // Device A re-subscribes, permission should still be valid:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  prv_assert_event_service_subscribed(true);
}

void test_ble_hrm__revoke(void) {
  // Device A subscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // Expect permissions UI to be presented:
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  cl_assert_equal_b(true, ble_hrm_is_sharing_to_connection(&s_conn_a));
  cl_assert_equal_b(true, ble_hrm_is_sharing());
  prv_assert_event_service_subscribed(true);

  // Revoke:
  ble_hrm_revoke_sharing_permission_for_connection(&s_conn_a);

  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));
  cl_assert_equal_b(false, ble_hrm_is_sharing());
  prv_assert_event_service_subscribed(false);
  prv_assert_last_disconnected(s_device_a);
}

void test_ble_hrm__revoke_all(void) {
  // Device A subscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // Expect permissions UI to be presented:
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  // Device B subscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_b, true);

  // Expect permissions UI to be presented:
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  ble_hrm_revoke_all();

  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));
  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_b));
  cl_assert_equal_b(false, ble_hrm_is_sharing());
  prv_assert_event_service_subscribed(false);
}

void test_ble_hrm__revoke_after_disconnection(void) {
  ble_hrm_revoke_sharing_permission_for_connection(nullptr);

  s_connections[0] = nullptr;
  ble_hrm_revoke_sharing_permission_for_connection(&s_conn_a);

  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(nullptr));

  // Shouldn't crash or anything
}

void test_ble_hrm__grant_after_disconnection(void) {
  // Device A subscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // Fake disconnection:
  s_connections[0] = nullptr;

  // Grabt permission after disconnection.
  // Request object should be freed and thing shouldn't crash.
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));
}

void test_ble_hrm__decline_permission_dont_ask_again_even_after_reconnecting(void) {
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // Decline:
  prv_assert_permissions_ui_and_respond(false /* is_granted */);

  // Unsub, resub:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, false);
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // No sharing request UI:
  cl_assert_equal_i(ble_hrm_push_sharing_request_window_fake.call_count, 0);

  // Fake disconnection:
  ble_hrm_handle_disconnection(&s_conn_a);

  // Fake reconn & subscribe:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);

  // No sharing request UI:
  cl_assert_equal_i(ble_hrm_push_sharing_request_window_fake.call_count, 0);

  // Still declined:
  cl_assert_equal_b(false, ble_hrm_is_sharing_to_connection(&s_conn_a));
}

void test_ble_hrm__unsub_upon_deinit(void) {
  // Device A subscribes:
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  // __cleanup() will do the deinit and also assert that  there's no subscription to the HRM mgr.
}

// Test that we handle a races where a subscription/disconnection callback happens in after
// deiniting the stack:
void test_ble_hrm__sub_after_deinit(void) {
  ble_hrm_deinit();

  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);
  prv_assert_event_service_subscribed(false);
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 0);

  ble_hrm_handle_disconnection(&s_conn_a);
  prv_assert_event_service_subscribed(false);
  cl_assert_equal_i(hrm_manager_subscribe_with_callback_fake.call_count, 0);

  ble_hrm_init(); // reinit, __cleanup() will deinit again
}

static void prv_put_and_assert_hrm_event(HRMEventType subtype, uint8_t bpm, HRMQuality quality,
                                         bool expect_pbl_bt_cb, bool expected_is_on_wrist) {
  int call_count_before = pbl_bt_hrm_service_handle_measurement_fake.call_count;

  PebbleEvent hrm_event = {
    .type = PEBBLE_HRM_EVENT,
    .hrm = {
      .event_type = subtype,
      .bpm = {
        .bpm = bpm,
        .quality = quality,
      },
    },
  };
  event_put(&hrm_event);
  fake_event_service_handle_last();

  if (expect_pbl_bt_cb) {
    cl_assert_equal_i(call_count_before + 1, pbl_bt_hrm_service_handle_measurement_fake.call_count);
    cl_assert_equal_i(bpm, s_last_ble_hrm_measurement.bpm);
    cl_assert_equal_b(expected_is_on_wrist, s_last_ble_hrm_measurement.is_on_wrist);
  } else {
    cl_assert_equal_i(call_count_before, pbl_bt_hrm_service_handle_measurement_fake.call_count);
  }
}

void test_ble_hrm__handle_hrm_event(void) {
  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);
  cl_assert_equal_i(0, pbl_bt_hrm_service_handle_measurement_fake.call_count);
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  // Don't grant permission to device B:
  pbl_bt_cb_hrm_service_update_subscription(s_device_b, true);
  prv_assert_permissions_ui_and_respond(false /* is_granted */);

  prv_put_and_assert_hrm_event(HRMEvent_BPM, 80, HRMQuality_Excellent,
                               true /* expect bt driver cb */, true /* expected_is_on_wrist */);

  // Assert only device A is listed as "permitted device" and B is not:
  cl_assert_equal_i(1, pbl_bt_hrm_service_handle_measurement_fake.arg2_val);
  cl_assert_equal_m(&s_last_permitted_devices[0], s_device_a, sizeof(*s_device_a));

  prv_put_and_assert_hrm_event(HRMEvent_BPM, 80, HRMQuality_OffWrist,
                               true /* expect bt driver cb */, false /* expected_is_on_wrist */);

  // Ignore non-BPM event:
  prv_put_and_assert_hrm_event(HRMEvent_HRV, 80, HRMQuality_OffWrist,
                               false /* expect bt driver cb */, false /* expected_is_on_wrist */);
}

void test_ble_hrm__handle_activity_pref_hrm_changes(void) {
  cl_assert_equal_i(0, pbl_bt_hrm_service_enable_fake.call_count);
  ble_hrm_handle_activity_prefs_heart_rate_is_enabled(false);
  cl_assert_equal_i(1, pbl_bt_hrm_service_enable_fake.call_count);
  cl_assert_equal_b(false, pbl_bt_hrm_service_enable_fake.arg0_val);

  // Disabled, again -- would lead to another call to pbl_bt_hrm_service_enable(),
  // the BT driver lib keeps track of whether it's enabled and is expected to ignore the call.
  ble_hrm_handle_activity_prefs_heart_rate_is_enabled(false);
  cl_assert_equal_i(2, pbl_bt_hrm_service_enable_fake.call_count);
  cl_assert_equal_b(false, pbl_bt_hrm_service_enable_fake.arg0_val);

  // Enable
  ble_hrm_handle_activity_prefs_heart_rate_is_enabled(true);
  cl_assert_equal_i(3, pbl_bt_hrm_service_enable_fake.call_count);
  cl_assert_equal_b(true, pbl_bt_hrm_service_enable_fake.arg0_val);
}

void test_ble_hrm__popup_after_long_continuous_use(void) {
  extern RegularTimerInfo *ble_hrm_timer(void);
  RegularTimerInfo *timer = ble_hrm_timer();

  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);
  prv_assert_permissions_ui_and_respond(true /* is_granted */);

  cl_assert_equal_b(true, regular_timer_is_scheduled(timer));

  pbl_bt_cb_hrm_service_update_subscription(s_device_a, false);
  cl_assert_equal_b(false, regular_timer_is_scheduled(timer));

  pbl_bt_cb_hrm_service_update_subscription(s_device_a, true);
  cl_assert_equal_b(true, regular_timer_is_scheduled(timer));

  cl_assert_equal_i(0, ble_hrm_push_reminder_popup_fake.call_count);
  fake_regular_timer_trigger(timer);
  cl_assert_equal_i(1, ble_hrm_push_reminder_popup_fake.call_count);

  // Except timer to be rescheduled again:
  cl_assert_equal_b(true, regular_timer_is_scheduled(timer));

  pbl_bt_cb_hrm_service_update_subscription(s_device_a, false);
  cl_assert_equal_b(false, regular_timer_is_scheduled(timer));
}
