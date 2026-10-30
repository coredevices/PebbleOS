/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/gap_le_connect.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/services/analytics/analytics.h>

#include <clar.h>
#include <comm/ble/gap_le_connect.h>
#include <comm/ble/gap_le_connection.h>
#include <kernel/events.h>

// Fakes
///////////////////////////////////////////////////////////

#include <fake_GAPAPI.h>
#include <fake_HCIAPI.h>
#include <fake_events.h>
#include <fake_new_timer.h>
#include <fake_pbl_malloc.h>
#include <fake_system_task.h>

// Stubs
///////////////////////////////////////////////////////////

// The "Pebble as Master" tests are disabled: role switching is not implemented and the FW always
// acts as slave.

static bool s_has_bond;

int pbl_bt_bond_get(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_info *info) {
  if (!s_has_bond) {
    return -1;
  }

  if (info) {
    *info = (struct pbl_bt_bond_info){
      .addr = *addr,
      .irk = *(const struct pbl_bt_sm_key *)fake_GAPAPI_get_fake_irk(),
      .irk_valid = true,
    };
  }

  return 0;
}

void cc2564A_bad_le_connection_complete_handle(
    unsigned int stack_id, const GAP_LE_Current_Connection_Parameters_t *params) {
}

const GAP_LE_Pairing_Capabilities_t *gap_le_pairing_capabilities(void) {
  return nullptr;
}

void gap_le_device_name_request(uintptr_t stack_id, GAPLEConnection *connection) {
}

void gatt_service_changed_server_cleanup_by_connection(GAPLEConnection *connection) {
}

void pbl_bt_handle_le_conn_params_update_event(
    const struct pbl_bt_conn_update_complete_event *event) {
}

struct pbl_bt_pairing_confirm_ctx;

void pbl_bt_pps_handle_status_change(const GAPLEConnection *connection) {
}

void pbl_bt_cb_pairing_confirm_handle_request(const struct pbl_bt_pairing_confirm_ctx *ctx,
                                              const char *device_name,
                                              const char *confirmation_token) {
}

void pbl_bt_cb_pairing_confirm_handle_completed(const struct pbl_bt_pairing_confirm_ctx *ctx,
                                                bool success) {
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

void bluetooth_analytics_handle_connection_disconnection_event(
    AnalyticsEvent type, uint8_t reason, const struct pbl_bt_remote_version_info *vers_info) {
}

// Helpers
///////////////////////////////////////////////////////////

static struct pbl_bt_device_internal prv_dummy_device(uint8_t octet) {
  struct pbl_bt_addr address = {
    .octets = {
      [0] = octet,
      [1] = octet,
      [2] = octet,
      [3] = octet,
      [4] = octet,
      [5] = octet,
    },
  };
  struct pbl_bt_device device = bt_device_init_with_address(address, true /* is_random */);
  return *(struct pbl_bt_device_internal *)(&device);
}

static struct pbl_bt_device_internal prv_add_bonding_for_fake_resolvable_device(void) {
  s_has_bond = true;
  return (struct pbl_bt_device_internal){};
}

static void prv_assert_no_event(void) {
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_NULL_EVENT);
}

static void prv_fake_connect(const struct pbl_bt_device_internal *device, bool is_master) {
  // Simulate getting a Connection Complete event for the device from Bluetopia:
  fake_gap_put_connection_event(HCI_ERROR_CODE_SUCCESS, is_master, device);
  cl_assert_equal_b(gap_le_connection_is_connected(device), true);
}

static void prv_fake_disconnect(const struct pbl_bt_device_internal *device, bool is_master) {
  fake_gap_put_disconnection_event(HCI_ERROR_CODE_SUCCESS,
                                   HCI_ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST, is_master,
                                   device);
  cl_assert_equal_b(gap_le_connection_is_connected(device), false);
}

static void prv_assert_connection_event(const struct pbl_bt_device_internal *device, bool connected,
                                        uint8_t hci_reason) {
  // Verify the Pebble event:
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_BLE_CONNECTION_EVENT);
  // Event should only go to KernelMain:
  cl_assert_equal_i(event.task_mask, (PebbleTaskBitset) ~(1 << PebbleTask_KernelMain));
  const PebbleBLEConnectionEvent *conn_event = &event.bluetooth.le.connection;
  const struct pbl_bt_device_internal event_device = PebbleEventToBTDeviceInternal(conn_event);
  const bool is_same_device = bt_device_equal(&event_device.opaque, &device->opaque);
  cl_assert_equal_b(is_same_device, true);
  cl_assert_equal_b(conn_event->connected, connected);
  cl_assert_equal_i(conn_event->hci_reason, hci_reason);
}

// Tests
///////////////////////////////////////////////////////////
extern void gap_le_connect_bluetopia_connection_callback(unsigned int stack_id,
                                                         GAP_LE_Event_Data_t *event_data,
                                                         unsigned long CallbackParameter);
void test_gap_le_connect__initialize(void) {
  fake_GAPAPI_init();

  // Register slave connection event callback for tests involving Pebble as slave:
  // This normally happens in gap_le_advert.c. Taking a shortcut to avoid dragging in more code.
  GAP_LE_Advertising_Enable(1, TRUE, nullptr, nullptr, gap_le_connect_bluetopia_connection_callback,
                            0);
  fake_event_init();
  s_has_bond = false;
  gap_le_connection_init();
  gap_le_connect_init();
}

void test_gap_le_connect__cleanup(void) {
  // Cancel all connection intents:
  gap_le_connect_cancel_all();

  gap_le_connect_deinit();

  cl_assert_equal_b(gap_le_connect_has_pending_create_connection(), false);
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 0);

  gap_le_connection_deinit();

  cl_assert_equal_b(fake_HCIAPI_whitelist_error_count(), 0);
  fake_HCIAPI_deinit();
}

// -----------------------------------------------------------------------------
// Parameter / Bounds checking

void test_gap_le_connect__register_max_intents(void) {
  for (int i = 0; i < GAP_LE_CONNECT_MASTER_MAX_CONNECTION_INTENTS + 1; ++i) {
    struct pbl_bt_device_internal device = prv_dummy_device(i);
    enum pbl_bt_errno e =
        gap_le_connect_connect(&device, true /* auto_reconnect */, false /* is_pairing_required */);

    if (i == GAP_LE_CONNECT_MASTER_MAX_CONNECTION_INTENTS) {
      // When the limit is reached, expect "not enough resources" error:
      cl_assert_equal_i(e, PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES);
    } else {
      cl_assert_equal_i(e, PBL_BT_ERRNO_OK);
      const bool registered = gap_le_connect_has_connection_intent(&device);
      cl_assert_equal_b(registered, true);
    }
  }
}

void test_gap_le_connect__register_null_device(void) {
  enum pbl_bt_errno e =
      gap_le_connect_connect(nullptr, true /* auto_reconnect */, false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_PARAMETER);
}

void test_gap_le_connect__unregister_null_device(void) {
  enum pbl_bt_errno e = gap_le_connect_cancel(nullptr);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_PARAMETER);
}

void test_gap_le_connect__register_invalid_bonding(void) {
  enum pbl_bt_errno e = gap_le_connect_connect_by_bond(nullptr, true /* auto_reconnect */,
                                                       false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_PARAMETER);
}

void test_gap_le_connect__register_non_existing_bonding(void) {
  const struct pbl_bt_device_internal addr = prv_dummy_device(1);
  enum pbl_bt_errno e = gap_le_connect_connect_by_bond(&addr, true /* auto_reconnect */,
                                                       false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_PARAMETER);
}

void test_gap_le_connect__unregister_invalid_bonding(void) {
  enum pbl_bt_errno e = gap_le_connect_cancel_by_bond(nullptr);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_PARAMETER);
}

void test_gap_le_connect__unregister_non_existing_bonding(void) {
  const struct pbl_bt_device_internal addr = prv_dummy_device(1);
  enum pbl_bt_errno e = gap_le_connect_cancel_by_bond(&addr);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_PARAMETER);
}

void test_gap_le_connect__register_is_already_registered(void) {
  enum pbl_bt_errno e;
  bool registered;
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  e = gap_le_connect_connect(&device, true /* auto_reconnect */, false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);
  registered = gap_le_connect_has_connection_intent(&device);
  cl_assert_equal_b(registered, true);

  // Try registering the device again:
  e = gap_le_connect_connect(&device, true /* auto_reconnect */, false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_STATE);

  // Should still be registered from the first call:
  registered = gap_le_connect_has_connection_intent(&device);
  cl_assert_equal_b(registered, true);
}

void test_gap_le_connect__register_same_device_and_bonding(void) {
  // Test that it is possible to have 2 intents for the same device, when registering one intent
  // using the resolvable address and one with a bonding. Pebble will not try to collate these,
  // because there are many addresses that resolve to the same bonding. The current implementation
  // uses one address or one bonding per intent.
  enum pbl_bt_errno e;
  bool registered;
  struct pbl_bt_device_internal device = *fake_GAPAPI_get_device_resolving_to_fake_irk();
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  // Register connection intent:
  e = gap_le_connect_connect(&device, true /* auto_reconnect */, false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);
  registered = gap_le_connect_has_connection_intent(&device);
  cl_assert_equal_b(registered, true);

  // Register another connection intent using the bonding:
  e = gap_le_connect_connect_by_bond(&bond, true /* auto_reconnect */,
                                     false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);
  registered = gap_le_connect_has_connection_intent_for_bond(&bond);
  cl_assert_equal_b(registered, true);
}

void test_gap_le_connect__unregister_unknown_device(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);
  enum pbl_bt_errno e = gap_le_connect_cancel(&device);
  cl_assert_equal_i(e, PBL_BT_ERRNO_INVALID_PARAMETER);
}

// -----------------------------------------------------------------------------
// Virtual (dis)connection events

void __disabled_test_gap_le_connect__connection_event_for_registered_client(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  enum pbl_bt_errno e =
      gap_le_connect_connect(&device, true /* auto_reconnect */, false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  // Device isn't connected. Verify no event was caused as a result of the
  // registration.
  prv_assert_no_event();

  // Connect & verify the event is sent:
  prv_fake_connect(&device, true /* is_master*/);
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);

  // Disconnect & verify the event is sent:
  prv_fake_disconnect(&device, true /* is_master */);
  prv_assert_connection_event(&device, false /* connected */,
                              HCI_ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST);
}

void test_gap_le_connect__connection_event_for_registered_client_by_bonding(void) {
  struct pbl_bt_device_internal device = *fake_GAPAPI_get_device_resolving_to_fake_irk();
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  // Register connection intent:
  enum pbl_bt_errno e = gap_le_connect_connect_by_bond(&bond, true /* auto_reconnect */,
                                                       false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  // Device isn't connected. Verify no event was caused as a result of the
  // registration.
  prv_assert_no_event();

  // Connect & verify the event is sent:
  prv_fake_connect(&device, false /* is_master*/);
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);

  // Disconnect & verify the event is sent:
  prv_fake_disconnect(&device, false /* is_master */);
  prv_assert_connection_event(&device, false /* connected */,
                              HCI_ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST);
}

void __disabled_test_gap_le_connect__register_for_already_connected_device(void) {
  enum pbl_bt_errno e;
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  e = gap_le_connect_connect(&device, true /* auto_reconnect */, false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  // Simulate getting a Connection Complete event for the device from Bluetopia:
  prv_fake_connect(&device, true /* is_master*/);

  // Verify the (virtual) connection event:
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);
}

void test_gap_le_connect__register_for_already_connected_bonding(void) {
  enum pbl_bt_errno e;
  struct pbl_bt_device_internal device = *fake_GAPAPI_get_device_resolving_to_fake_irk();
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  // Simulate getting a Connection Complete event for the device from Bluetopia:
  prv_fake_connect(&device, false /* is_master*/);
  fake_event_clear_last();

  // Register connection intent:
  e = gap_le_connect_connect_by_bond(&bond, true /* auto_reconnect */,
                                     false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  // Verify the (virtual) connection event:
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);
}

void __disabled_test_gap_le_connect__disconnection_event_upon_airplane_mode(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  enum pbl_bt_errno e =
      gap_le_connect_connect(&device, true /* auto_reconnect */, false /* is_pairing_required */);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  prv_fake_connect(&device, true /* is_master*/);

  // Air-plane mode:
  gap_le_connect_deinit();

  // Verify the (virtual) disconnection event:
  prv_assert_connection_event(&device, false /* connected */,
                              GAPLEConnectHCIReasonExtensionAirPlaneMode);
}

// -----------------------------------------------------------------------------
// Auto-reconnect Tests

void __disabled_test_gap_le_connect__single_client_no_autoreconnect(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  gap_le_connect_connect(&device, false /* no auto_reconnect */, false /* is_pairing_required */);

  prv_fake_connect(&device, true /* is_master*/);

  // Verify the (virtual) connection event:
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);

  prv_fake_disconnect(&device, true /* is_master */);

  // Verify the (virtual) disconnection event:
  prv_assert_connection_event(&device, false /* connected */,
                              HCI_ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST);

  // Verify that the connection intent has been removed after disconnection:
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 0);
}

// -----------------------------------------------------------------------------
// Cancel Connect (as Master)

void __disabled_test_gap_le_connect__cancel_connect(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  gap_le_connect_connect(&device, false /* no auto_reconnect */, false /* is_pairing_required */);

  enum pbl_bt_errno e = gap_le_connect_cancel(&device);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  // The LE Cancel Create Connection command is always followed by an event that
  // is sent by the BT Controller. Simulate this event:
  fake_gap_le_put_cancel_create_event(&device, true /* is_master */);

  prv_assert_no_event();

  // Verify there are no more intents:
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 0);
  cl_assert_equal_b(gap_le_connect_has_pending_create_connection(), false);
}

void __disabled_test_gap_le_connect__disconnection_event_upon_cancel_connect(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  gap_le_connect_connect(&device, false /* no auto_reconnect */, false /* is_pairing_required */);

  prv_fake_connect(&device, false /* is_master*/);

  enum pbl_bt_errno e = gap_le_connect_cancel(&device);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  // The LE Cancel Create Connection command is always followed by an event that
  // is sent by the BT Controller. Simulate this event:
  fake_gap_le_put_cancel_create_event(&device, true /* is_master */);

  // Verify the (virtual) disconnection event:
  prv_assert_connection_event(&device, false /* connected */,
                              GAPLEConnectHCIReasonExtensionCancelConnect);
  // Verify there are no more intents:
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 0);
}

// -----------------------------------------------------------------------------
// Cancel Connect by Bonding (as Slave)

void test_gap_le_connect__slave_cancel_connect_by_bonding(void) {
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  // Register connection intent:
  gap_le_connect_connect_by_bond(&bond, false /* no auto_reconnect */,
                                 false /* is_pairing_required */);

  enum pbl_bt_errno e = gap_le_connect_cancel_by_bond(&bond);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  prv_assert_no_event();

  // Verify there are no more intents:
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 0);
  cl_assert_equal_b(gap_le_connect_has_pending_create_connection(), false);
}

void test_gap_le_connect__slave_disconnection_event_upon_cancel_connect_by_bonding(void) {
  struct pbl_bt_device_internal device = *fake_GAPAPI_get_device_resolving_to_fake_irk();
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  prv_fake_connect(&device, false /* is_master*/);

  // Register connection intent:
  gap_le_connect_connect_by_bond(&bond, false /* no auto_reconnect */,
                                 false /* is_pairing_required */);

  enum pbl_bt_errno e = gap_le_connect_cancel_by_bond(&bond);
  cl_assert_equal_i(e, PBL_BT_ERRNO_OK);

  // Verify the (virtual) disconnection event:
  prv_assert_connection_event(&device, false /* connected */,
                              GAPLEConnectHCIReasonExtensionCancelConnect);
  // Verify there are no more intents:
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 0);
}

// -------------------------------------------------------------------------------------------------
// Pairing

void __disabled_test_gap_le_connect__one_shot_intent_removed_when_disconnected_before_encrypt(
    void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register one-shot connection intent, with pairing required:
  gap_le_connect_connect(&device, false /* no auto_reconnect */, true /* is_pairing_required */);

  // Expect intent:
  cl_assert_equal_b(gap_le_connect_has_connection_intent(&device), true);

  prv_fake_connect(&device, true /* is_master*/);
  prv_fake_disconnect(&device, true /* is_master */);

  // Expect intent to be removed:
  cl_assert_equal_b(gap_le_connect_has_connection_intent(&device), false);
}

void test_gap_le_connect__connection_event_only_after_encrypted_if_encryption_required(void) {
  struct pbl_bt_device_internal device = *fake_GAPAPI_get_device_resolving_to_fake_irk();
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  // Register connection intent:
  gap_le_connect_connect_by_bond(&bond, true /* no auto_reconnect */,
                                 true /* is_pairing_required */);

  prv_fake_connect(&device, false /* is_master*/);

  // Verify there is NO (virtual) connection event, the link is not encrypted yet:
  // TODO: legacy PBL_BT_PEBBLE_CONNECTION_EVENT is still emitted, see gap_le_connect.c
  // prv_put_legacy_connection_event.
  //
  // prv_assert_no_event();

  fake_event_clear_last();
  fake_GAPAPI_set_encrypted_for_device(&device);
  fake_GAPAPI_put_encryption_change_event(true /* encrypted */, GAP_LE_PAIRING_STATUS_NO_ERROR,
                                          false /* is_master */, &device);

  // Verify the (virtual) connection event:
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);
}

void test_gap_le_connect__add_intent_requiring_pairing_after_connected_and_encrypted(void) {
  struct pbl_bt_device_internal device = *fake_GAPAPI_get_device_resolving_to_fake_irk();
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  prv_fake_connect(&device, false /* is_master*/);
  fake_GAPAPI_set_encrypted_for_device(&device);
  fake_GAPAPI_put_encryption_change_event(true /* encrypted */, GAP_LE_PAIRING_STATUS_NO_ERROR,
                                          false /* is_master */, &device);
  fake_event_clear_last();

  const BleAddressAndIRKChange e = {
    .device = device,
    .is_address_updated = true,
    .new_device = device,
    .is_resolved = true,
    .irk = *(const struct pbl_bt_sm_key *)fake_GAPAPI_get_fake_irk(),
  };
  pbl_bt_handle_le_connection_handle_update_address_and_irk(&e);

  gap_le_connection_by_device(&device);

  // Register connection intent:
  gap_le_connect_connect_by_bond(&bond, true /* no auto_reconnect */,
                                 true /* is_pairing_required */);

  // Verify the (virtual) connection event:
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);
}

// -----------------------------------------------------------------------------
// Handling Bonding Changes

void test_gap_le_connect__removed_bonding_while_connected(void) {
  struct pbl_bt_device_internal device = *fake_GAPAPI_get_device_resolving_to_fake_irk();
  const struct pbl_bt_device_internal bond = prv_add_bonding_for_fake_resolvable_device();

  // Register connection intent:
  gap_le_connect_connect_by_bond(&bond, false /* no auto_reconnect */,
                                 false /* is_pairing_required */);

  prv_fake_connect(&device, false /* is_master*/);

  // Verify the (virtual) connection event:
  prv_assert_connection_event(&device, true /* connected */, HCI_ERROR_CODE_SUCCESS);

  // Simulate the bond deleted callback:
  gap_le_connect_handle_bond_deleted(&bond);

  // Verify the (virtual) disconnection event:
  prv_assert_connection_event(&device, false /* connected */,
                              GAPLEConnectHCIReasonExtensionUserRemovedBonding);
  // Verify there are no more intents:
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 0);
}

// -----------------------------------------------------------------------------
// BT Controller White-list management

void __disabled_test_gap_le_connect__whitelist_add_when_disconnected(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  gap_le_connect_connect(&device, false /* no auto_reconnect */, false /* is_pairing_required */);

  // Not connected yet, so expect to be added to white-list:
  cl_assert_equal_b(fake_HCIAPI_whitelist_contains(&device), true);

  prv_fake_connect(&device, true /* is_master*/);

  // Connected, so expect to be removed from white-list:
  cl_assert_equal_b(fake_HCIAPI_whitelist_contains(&device), false);
}

void __disabled_test_gap_le_connect__whitelist_remove_when_connected(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);

  // Register connection intent:
  gap_le_connect_connect(&device, false /* no auto_reconnect */, false /* is_pairing_required */);
  prv_fake_connect(&device, true /* is_master*/);

  gap_le_connect_cancel(&device);

  // Connected, so expect to be removed from white-list:
  cl_assert_equal_b(fake_HCIAPI_whitelist_contains(&device), false);
}

void __disabled_test_gap_le_connect__whitelist_repopulated_on_init(void) {
  struct pbl_bt_device_internal device = prv_dummy_device(1);
  gap_le_connect_connect(&device, false /* no auto_reconnect */, false /* is_pairing_required */);

  gap_le_connect_deinit();

  // Connection intents survive air-plane mode:
  cl_assert_equal_i(gap_le_connect_connection_intents_count(), 1);

  // "Reset" BT Controller:
  fake_HCIAPI_deinit();
  cl_assert_equal_i(fake_HCIAPI_whitelist_count(), 0);

  gap_le_connect_init();

  // Not connected yet, so expect to be added to white-list:
  cl_assert_equal_b(fake_HCIAPI_whitelist_contains(&device), true);
  cl_assert_equal_i(fake_HCIAPI_whitelist_count(), 1);
}
