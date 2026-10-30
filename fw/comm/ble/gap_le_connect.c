/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_connect.h"

#include "gap_le_advert.h"
#include "gap_le_connect_params.h"
#include "gap_le_connection.h"

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/gap_le_connect.h>
#include <pbl/bluetooth/pebble_pairing_service.h>
#include <pbl/btutil/bt_device.h>
#include <pbl/btutil/sm_util.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/ble_hrm.h>

#include <comm/bluetooth_analytics.h>
#include <comm/bt_conn_mgr.h>
#include <comm/bt_lock.h>
#include <kernel/events.h>
#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

//! About this module
//! -----------------
//! - Manages initiating connections to other BLE devices as a Master.
//! - Handles inbound connection events as Slave as well.
//! - Programs the Bluetooth controller's white-list with the device(s) to
//!   initiate connections to.
//! - Uses the Bluetooth controller operations "LE Create Connection" and
//!   "LE Create Connection Cancel" to start/stop initiating, using the
//!   white-list as set of devices to look out for.
//! - Exposes an internal API that lets the kernel register "connection intents".
//! - Connection intents survive airplane mode.
//! - Connection events are virtualized: if an intent is registered for a device
//!   that is already connected, a connection event is sent (pretty much
//!   immediately) as if the device just connected.
//!
//! BT 4.1 Questions
//! ----------------
//! - What happens when LE Create Connection is sent for device that is already
//!   connected as master?
//!
//! - What happens when whitelisting a resolvable address, then connecting and
//!   finding out the device is already connected?

//! Data structure to hold cached bonding info
typedef struct {
  struct pbl_bt_device_internal device; // containing identity address, not connection address
  struct pbl_bt_sm_key irk;
} GAPLEConnectionIntentBonding;

//! Intent to connect.
typedef struct {
  ListNode node;

  //! The device to connect to.
  //! @note When using a bonding, its address will be set to the last known connection address.
  struct pbl_bt_device_internal device;

  //! True if the system should handle pairing / encryption reestablishment
  //! transparently first, before sending the connection event.
  bool is_pairing_required;

  //! True if the intent should be kept around until gap_le_connect_cancel() is
  //! called, false if the intent should be removed when the slave device
  //! disconnects.
  bool auto_reconnect;

  //! True if a connection event has been sent. False if a disconnection event
  //! has been sent or if an event has never been sent.
  //! With is_pairing_required set, the connection event only gets sent after
  //! pairing/encryption is established.
  bool connected;

  //! True when `bonding` exists
  bool is_bonding_based;

  //! The optional bonding info for the device to connect to. NULL when unused.
  GAPLEConnectionIntentBonding bonding[];
} GAPLEConnectionIntent;

static_assert(offsetof(GAPLEConnectionIntent, node) == 0,
              "ListNode must be the first field in GAPLEConnectionIntent");

typedef enum {
  GAPLEConnectionEventDisconnected,
  GAPLEConnectionEventConnectedNotEncrypted,
  GAPLEConnectionEventConnectedAndEncrypted,
} GAPLEConnectionEvent;

//! Value indicating the current BLE connectivity role to the phone, from Pebble's point of view.
typedef enum {
  GAPLERoleSlave,
  GAPLERoleMaster,
} GAPLERole;

// -------------------------------------------------------------------------------------------------
// Static Variables -- MUST be protected with bt_lock/unlock!

//! The list of connection intents.
static GAPLEConnectionIntent *s_intents;

//! True if there is a pending LE Create Connection call, false if not.
static bool s_has_pending_create_connection;

//! True if the device is currently connected as LE Slave (4.0)
static bool s_is_connected_as_slave;

//! Role switching is not implemented: always act as slave (never perform LE Create Connection).
static const GAPLERole s_current_role = GAPLERoleSlave;

// -------------------------------------------------------------------------------------------------
// Function prototypes

typedef void (*IntentApply)(GAPLEConnectionIntent *intent, void *data);
static void prv_apply_fuction_to_intents_matching_connection(const GAPLEConnection *connection,
                                                             IntentApply fp, void *data);

static bool prv_intent_matches_connection(const GAPLEConnectionIntent *intent,
                                          const GAPLEConnection *connection);
static void prv_start_connecting_if_needed(void);
static GAPLEConnectionIntent *prv_get_intent_by_device(const struct pbl_bt_device_internal *device);
static void prv_intent_remove_and_free(GAPLEConnectionIntent *intent);
static bool prv_is_intent_using_whitelist(const GAPLEConnectionIntent *intent);
static void prv_mutate_whitelist(const struct pbl_bt_device_internal *device, bool is_adding);
static void prv_mutate_whitelist_safely(const struct pbl_bt_device_internal *device,
                                        bool is_adding);

// -------------------------------------------------------------------------------------------------

// Only used by the Settings/Bluetooth UI to refresh the list.
static void prv_put_legacy_connection_event(const struct pbl_bt_device_internal *device,
                                            bool is_connected) {
  PebbleEvent event = {
    .type = PBL_BT_PEBBLE_CONNECTION_EVENT,
    .bluetooth = {
      .connection = {
        .is_ble = true,
        .device = *device,
      },
    },
  };

  if (is_connected) {
    event.bluetooth.connection.state = PebbleBluetoothConnectionEventStateConnected;
  } else {
    event.bluetooth.connection.state = PebbleBluetoothConnectionEventStateDisconnected;
  }
  event_put(&event);
}

// -------------------------------------------------------------------------------------------------

static void prv_put_connection_event(const struct pbl_bt_device_internal *device,
                                     uint8_t hci_reason, bool connected) {
  PebbleEvent pebble_event = {
    .type = PEBBLE_BLE_CONNECTION_EVENT,
    .task_mask = (PebbleTaskBitset) ~(1u << PebbleTask_KernelMain),
    .bluetooth = {
      .le = {
        .connection = {
          .bt_device_bits = device->opaque.opaque_64,
          .hci_reason = hci_reason,
          .connected = connected,
        },
      },
    },
  };
  event_put(&pebble_event);
}

// -------------------------------------------------------------------------------------------------

//! extern'd for gatt.c
//! @return true if there is a connection intent for the connection.
//! bt_lock is assumed to be taken before calling this function.
bool gap_le_connect_has_intent_for_connection(const GAPLEConnection *connection) {
  for (const GAPLEConnectionIntent *intent = s_intents; intent;
       intent = (const GAPLEConnectionIntent *)intent->node.next) {
    if (prv_intent_matches_connection(intent, connection)) {
      return true;
    }
  }
  return false;
}

// -------------------------------------------------------------------------------------------------
//! Updates the state of the intent and sends an event if the connection state changed.
//! @note Upon disconnection, this function also removes and free's the intent
//! if it does not auto-reconnect. The caller of this function should therefore
//! not attempt to access the intent after this function returns.
//! bt_lock is assumed to be taken before calling this function.
//! @return false if the intent has been cleaned-up by this function and should
//! not be accessed any longer after returning.
static bool prv_update_intent(GAPLEConnectionIntent *intent, uint8_t hci_reason,
                              GAPLEConnectionEvent event) {
  const bool connected = (event == GAPLEConnectionEventConnectedNotEncrypted ||
                          event == GAPLEConnectionEventConnectedAndEncrypted);

  // (One-shot) intents should survive air plane mode toggles:
  const bool is_done = (event == GAPLEConnectionEventDisconnected && !intent->auto_reconnect &&
                        hci_reason != GAPLEConnectHCIReasonExtensionAirPlaneMode);

  // If is_pairing_required is true, "connected & not encrypted" is an
  // in-between state that should not be reported.
  const bool is_hidden =
      (intent->is_pairing_required && event == GAPLEConnectionEventConnectedNotEncrypted);

  if (intent->connected != connected && !is_hidden) {
    intent->connected = connected;
    prv_put_connection_event(&intent->device, hci_reason, connected);
  }

  if (is_done) {
    prv_intent_remove_and_free(intent);
    return false;
  }
  return true;
}

void pbl_bt_handle_le_connection_handle_update_address(const struct pbl_bt_addr_change *e) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_device(&e->device);
    if (!connection) {
      PBL_LOG_ERR(
          "Got address update for non-existent connection. "
          "Old addr:" PBL_BT_ADDR_FMT,
          PBL_BT_ADDR_XPLODE(e->device.address));
      goto unlock;
    }

    connection->device = e->new_device;
    PBL_LOG_INFO("Updated address to " PBL_BT_ADDR_FMT,
                 PBL_BT_ADDR_XPLODE(connection->device.address));
  }
unlock:
  bt_unlock();
}

void pbl_bt_handle_le_connection_handle_update_irk(const struct pbl_bt_irk_change *e) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_device(&e->device);
    if (!connection) {
      PBL_LOG_ERR("Got IRK update for non-existent connection");
      goto unlock;
    }

    if (connection->irk) {
      PBL_LOG_WRN("Connection already has IRK!?");
    }

    gap_le_connection_set_irk(connection, e->irk_valid ? &e->irk : nullptr);
  }
unlock:
  bt_unlock();
}

void pbl_bt_handle_peer_version_info_event(
    const struct pbl_bt_remote_version_info_received_event *e) {
  bt_lock();

  GAPLEConnection *connection = gap_le_connection_by_device(&e->peer_address);
  if (connection) {
    const struct pbl_bt_remote_version_info *info = &e->remote_version_info;
    connection->remote_version_info = *info;
    PBL_LOG_DBG("Remote Vers Info: VersNr: %d, CompId: 0x%x, SubVersNr: 0x%x",
                (int)info->version_number, (int)info->company_identifier,
                (int)info->subversion_number);
  }

  bt_unlock();
}

//! bt_lock is assumed to be taken before calling this function.
void pbl_bt_handle_le_connection_complete_event(const struct pbl_bt_conn_complete_event *event) {
  // Create timer outside of bt_lock to avoid deadlock with NimbleHost.
  // new_timer_create() acquires TaskTimerManager mutex, which may be held by NimbleHost
  // when it's trying to acquire bt_lock, leading to a lock ordering deadlock.
  TimerID param_watchdog_timer = TIMER_INVALID_ID;
  if (event->status == PBL_BT_HCI_STATUS_SUCCESS) {
    param_watchdog_timer = new_timer_create();
    if (!param_watchdog_timer) {
      PBL_LOG_ERR("Failed to create timer for connection params");
      return;
    }
  }

  bt_lock();
  const struct pbl_bt_conn_params *params = &event->conn_params;
  PBL_LOG_INFO("LE Conn Compl: addr=" PBL_BT_ADDR_FMT ", is_random_addr=%u,",
               PBL_BT_ADDR_XPLODE(event->peer_address.address),
               event->peer_address.is_random_address);
  PBL_LOG_INFO(
      "               hdl=%u, status=0x%02x, master=%u, %u, slave lat=%u, "
      "supervision timeout=%u, is_resolved=%c",
      event->handle, event->status, event->is_master, params->conn_interval_1_25ms,
      params->slave_latency_events, params->supervision_timeout_10ms,
      event->is_resolved ? 'Y' : 'N');

  // When an "LE Connection Complete" event is received, the
  // "LE Create Connection" operation is stopped, so update our state:
  s_has_pending_create_connection = false;

  switch (event->status) {
    case PBL_BT_HCI_STATUS_SUCCESS: {
      // New connection! Update our records:
      const bool local_is_master = event->is_master;

      if (!local_is_master) {
        s_is_connected_as_slave = true;
        gap_le_advert_handle_connect_as_slave();

        prv_put_legacy_connection_event(&event->peer_address, true /* connected */);
      }

      if (gap_le_connection_is_connected(&event->peer_address)) {
        // We have seen this crop up for cases where the phone has disconnected due to a timeout
        // but the watch has not yet. In practice, I think the only way it could happen is if a
        // user is sitting in the Bluetooth settings menu and walking in and out of range. If it
        // does take place, let's trigger a disconnect to try and put us back into a sane state
        PBL_LOG_ERR("Not adding connection for device. It is already connected .. disconnecting");
        pbl_bt_gap_le_disconnect(&event->peer_address);
        break;
      }

      const struct pbl_bt_sm_key *remote_irk = event->is_resolved ? &event->irk : nullptr;
      GAPLEConnection *connection = gap_le_connection_add(&event->peer_address, remote_irk,
                                                          local_is_master, param_watchdog_timer);
      param_watchdog_timer = TIMER_INVALID_ID; // Timer now owned by connection
      // Cache the BLE connection parameters
      connection->conn_params = *params;
      connection->gatt_mtu = event->mtu;

      bool found_match = false;
      GAPLEConnectionIntent *intent = s_intents;
      while (intent) {
        GAPLEConnectionIntent *next = (GAPLEConnectionIntent *)intent->node.next;
        if (prv_intent_matches_connection(intent, connection)) {
          found_match = true;

          if (intent->is_bonding_based) {
            // Update connection address:
            intent->device = event->peer_address;

            connection->is_bonded = true;
            connection->identity = intent->bonding->device;
          }

          prv_update_intent(intent, PBL_BT_HCI_STATUS_SUCCESS,
                            GAPLEConnectionEventConnectedNotEncrypted);

          if (prv_is_intent_using_whitelist(intent)) {
            // Remove from white-list, because the device is connected now.
            prv_mutate_whitelist(&event->peer_address, false /* remove */);
          }

          if (local_is_master && intent->is_pairing_required) {
            // TODO: kick off pairing
          }
        }
        intent = next;
      }

      if (!local_is_master) { // At the moment we don't grab analytics for connections we generate
        bluetooth_analytics_handle_connect(&event->peer_address, &event->conn_params);
      }

      if (!found_match) {
        // No intent from our end, e.g. a phone connecting for the first time.
        PBL_LOG_WRN("No intent for connection");
        bluetooth_analytics_handle_no_intent_for_connection();
      }

#ifdef CONFIG_RECOVERY_FW
      // In PRF, stick to shortest connection interval indefinitely:
      conn_mgr_set_ble_conn_response_time(connection, PBL_BT_CONSUMER_PRF, PBL_BT_RESPONSE_TIME_MIN,
                                          MAX_PERIOD_RUN_FOREVER);
#endif
      break;
    }

    case PBL_BT_HCI_STATUS_UNKNOWN_CONNECTION_IDENTIFIER: {
      // Happens if "Connection Create" was cancelled.
      // See Bluetooth Spec 4.0, Volume 2, Part E, Chapter 7.8.13.
      break;
    }

    default: {
      PBL_LOG_ERR("Connection Complete Event status: 0x%x", event->status);
      break;
    }
  }

  // Continue initiating connections to disconnected devices:
  prv_start_connecting_if_needed();
  bt_unlock();

  // Clean up timer if we didn't use it (e.g., connection failed or already connected)
  if (param_watchdog_timer != TIMER_INVALID_ID) {
    new_timer_delete(param_watchdog_timer);
  }
}

//! bt_lock is assumed to be taken before calling this function.
void pbl_bt_handle_le_disconnection_complete_event(
    const struct pbl_bt_disconn_complete_event *event) {
  bt_lock();

  switch (event->status) {
    case PBL_BT_HCI_STATUS_SUCCESS: {
      // Disconnection! Update our records:
      GAPLEConnection *connection = gap_le_connection_by_device(&event->peer_address);
      if (!connection) {
        // Stopping Bluetooth drops its links after the connections are gone.
        PBL_LOG_DBG("LE Disconn: no connection for hdl=%u", event->handle);
        break;
      }
#ifdef CONFIG_SERVICE_BLE_HRM
      ble_hrm_handle_disconnection(connection);
#endif
      const bool local_is_master = connection->local_is_master;

      PBL_LOG_INFO("LE Disconn: addr=" PBL_BT_ADDR_FMT ", is_random_addr=%u,",
                   PBL_BT_ADDR_XPLODE(event->peer_address.address),
                   event->peer_address.is_random_address);
      PBL_LOG_INFO("            hdl=%u, status=0x%02x, reason=0x%02x, master=%u", event->handle,
                   event->status, event->reason, local_is_master);

      bluetooth_analytics_handle_disconnect(local_is_master);

      bluetooth_analytics_handle_connection_disconnection_event(event->reason,
                                                                &connection->remote_version_info);

      if (!local_is_master) {
        s_is_connected_as_slave = false;
        gap_le_advert_handle_disconnect_as_slave();

        prv_put_legacy_connection_event(&event->peer_address, false /* disconnected */);
      }

      GAPLEConnectionIntent *intent = s_intents;
      while (intent) {
        GAPLEConnectionIntent *next = (GAPLEConnectionIntent *)intent->node.next;
        if (prv_intent_matches_connection(intent, connection)) {
          if (prv_update_intent(intent, event->reason, GAPLEConnectionEventDisconnected)) {
            // Only if the intent hasn't been cleaned up by now:
            if (prv_is_intent_using_whitelist(intent)) {
              // Add to white-list, because the device is disconnected now and we
              // need to start connecting again:
              prv_mutate_whitelist_safely(&event->peer_address, true /* add */);
            }

            if (intent->is_bonding_based) {
              // Clear out connection address (more for debugging than any else):
              intent->device = (const struct pbl_bt_device_internal){};
            }
          }
        }
        intent = next;
      }

      gap_le_connection_remove(&event->peer_address);
      break;
    }

    default: {
      PBL_LOG_ERR("Disconnection Complete Event status: 0x%x", event->status);
      break;
    }
  }
  bt_unlock();
}

// -------------------------------------------------------------------------------------------------
//! Convenience function to apply a function to each intent matching or resolving to a device
//! bt_lock is assumed to be taken before calling this function.
static void prv_apply_fuction_to_intents_matching_connection(const GAPLEConnection *connection,
                                                             IntentApply fp, void *data) {
  GAPLEConnectionIntent *intent = s_intents;
  while (intent) {
    GAPLEConnectionIntent *next = (GAPLEConnectionIntent *)intent->node.next;
    if (prv_intent_matches_connection(intent, connection)) {
      fp(intent, data);
    }
    intent = next;
  }
}

// -------------------------------------------------------------------------------------------------
//! Helper for prv_handle_encryption_change

static void prv_send_encrypted_event(GAPLEConnectionIntent *intent, void *unused_data) {
  prv_update_intent(intent, PBL_BT_HCI_STATUS_SUCCESS, GAPLEConnectionEventConnectedAndEncrypted);
}

//! bt_lock is assumed to be taken before calling this function.
void pbl_bt_handle_le_encryption_change_event(const struct pbl_bt_encryption_change *event) {
  bt_lock();
  const bool is_encrypted = (event->encryption_enabled);
  if (!is_encrypted) {
    // The "Encryption Change" event can only enable encryption, there's no inverse,
    // so there must be an error:
    PBL_LOG_ERR("LE encryption change: failed (%u)", event->status);
    goto unlock;
  }

  // Bluetopia doesn't set the 'is_random_address' field in the encryption change event, so using
  // gap_le_connection_by_device() will fail.
  GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
  if (connection->is_encrypted) {
    PBL_LOG_INFO("LE encryption change: refreshed");
    goto unlock;
  }

  const bool local_is_master = connection->local_is_master;
  connection->is_encrypted = true;

  if (!local_is_master) {
    // The driver already logs encryption changes (status/encrypted/bonded)
    PBL_LOG_DBG("LE encryption change: encrypted");
    bluetooth_analytics_handle_encryption_change();
#ifdef CONFIG_BT_SERVICE_PPS
    pbl_bt_pps_handle_status_change(connection);
#endif
  }

  prv_apply_fuction_to_intents_matching_connection(connection, prv_send_encrypted_event, nullptr);
unlock:
  bt_unlock();
}

// -------------------------------------------------------------------------------------------------
//! Watch-driven connections are not implemented.
//! bt_lock is assumed to be taken before calling these functions.

static void prv_start_connecting(void) {
  PBL_LOG_WRN("Watch driven BLE connection unimplemented");
}

static void prv_stop_connecting(void) {
  PBL_LOG_WRN("Watch driven BLE connection cancel unimplemented");
}

static void prv_mutate_whitelist(const struct pbl_bt_device_internal *device, bool is_adding) {
  PBL_LOG_WRN("BLE whitelist mutation unimplemented");
}

// -------------------------------------------------------------------------------------------------
//! Helpers to manage the s_intents list.
//! bt_lock() is expected to be taken by the caller for each of these.

static bool prv_intent_matches_connection(const GAPLEConnectionIntent *intent,
                                          const GAPLEConnection *connection) {
  if (intent->is_bonding_based) {
    // If the bonding-based intent is connected, the `device` is set to the connection address,
    // if it's not connected, it's all zeroes.
    if (bt_device_internal_equal(&connection->device, &intent->device)) {
      return true;
    }
    if (!connection->irk) {
      // are we looking for a bonding which did not exchange an irk?
      if (sm_is_pairing_info_irk_not_used(&intent->bonding->irk)) {
        PBL_LOG_DBG("Bonding does not have irk ... comparing identity address");
        return (0 == memcmp(&connection->device.opaque, &intent->bonding->device.opaque,
                            sizeof(connection->device.opaque)));
      }

      return false;
    }
    return (0 == memcmp(connection->irk, &intent->bonding->irk, sizeof(*connection->irk)));
  } else {
    return bt_device_internal_equal(&connection->device, &intent->device);
  }
}

static void prv_intent_remove_and_free(GAPLEConnectionIntent *intent) {
  list_remove(&intent->node, (ListNode **)&s_intents, nullptr);
  kernel_free(intent);
}

static bool prv_intent_filter_by_device(ListNode *node, void *data) {
  const struct pbl_bt_device_internal *target_device = (const struct pbl_bt_device_internal *)data;
  const GAPLEConnectionIntent *intent = (const GAPLEConnectionIntent *)node;
  if (intent->is_bonding_based) {
    return false;
  }
  return bt_device_internal_equal(target_device, &intent->device);
}

static GAPLEConnectionIntent *prv_get_intent_by_device(
    const struct pbl_bt_device_internal *device) {
  return (GAPLEConnectionIntent *)list_find(&s_intents->node, prv_intent_filter_by_device,
                                            (void *)device);
}

static bool prv_intent_filter_by_bond(ListNode *node, void *data) {
  const struct pbl_bt_device_internal *addr = data;
  const GAPLEConnectionIntent *intent = (const GAPLEConnectionIntent *)node;
  if (!intent->is_bonding_based) {
    return false;
  }
  return bt_device_internal_equal(&intent->bonding->device, addr);
}

static GAPLEConnectionIntent *prv_get_intent_by_bond(const struct pbl_bt_device_internal *addr) {
  return (GAPLEConnectionIntent *)list_find(&s_intents->node, prv_intent_filter_by_bond,
                                            (void *)addr);
}

static bool prv_intent_filter_disconnected(ListNode *node, void *data) {
  const GAPLEConnectionIntent *intent = (const GAPLEConnectionIntent *)node;
  return !gap_le_connection_is_connected(&intent->device);
}

static bool prv_has_intents_for_disconnected_devices(void) {
  return list_find(&s_intents->node, prv_intent_filter_disconnected, nullptr);
}

static uint32_t prv_intents_count(void) {
  return list_count(&s_intents->node);
}

// -------------------------------------------------------------------------------------------------
static bool prv_is_intent_using_whitelist(const GAPLEConnectionIntent *intent) {
  // TODO: If the bonding does not contain a valid IRK, perhaps we should use and whitelist the
  // identity address and treat it as a normal connection intent?
  // See note in BT spec "Note: An all zero Identity Resolving Key data field indicates that a
  // device does not have a valid resolvable private address." in Security Manager chapter.
  return (!intent->is_bonding_based);
}

// -------------------------------------------------------------------------------------------------
static void prv_start_connecting_if_needed(void) {
  if (s_current_role == GAPLERoleSlave) {
    return;
  }
  if (prv_has_intents_for_disconnected_devices()) {
    prv_start_connecting();
  }
}

// -------------------------------------------------------------------------------------------------
//! Adds or removes a device to/from the Bluetooth controller's whitelist.
//! Stops and (re)starts the LE Create Connection operation as necessary.
static void prv_mutate_whitelist_safely(const struct pbl_bt_device_internal *device,
                                        bool is_adding) {
  // If there are already connection intents, cancel connecting briefly,
  // otherwise it's illegal to modify the white-list.
  prv_stop_connecting();

  // Mutate white-list:
  prv_mutate_whitelist(device, is_adding);

  // Start/continue connecting:
  prv_start_connecting_if_needed();
}

// -------------------------------------------------------------------------------------------------
// Helper data structure for prv_register_intent

struct RegisterIntentRequest {
  bool is_bonding_based;
  union {
    const struct pbl_bt_device_internal *device;
    GAPLEConnectionIntentBonding bonding;
  };
};

// -------------------------------------------------------------------------------------------------
//! Registers a connection intent
//! bt_lock() is expected to be taken by the caller
static enum pbl_bt_errno prv_register_intent(struct RegisterIntentRequest *request,
                                             bool auto_reconnect, bool is_pairing_required) {
  // Check if the max count wasn't exceeded:
  const uint32_t prev_num_intents = prv_intents_count();
  if (prev_num_intents >= GAP_LE_CONNECT_MASTER_MAX_CONNECTION_INTENTS) {
    return PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES;
  }

  bool is_already_connected = false;
  bool is_already_encrypted = false;

  const struct pbl_bt_device_internal *connected_device = nullptr;
  GAPLEConnectionIntent *intent;

  if (request->is_bonding_based) {
    const GAPLEConnection *connection = gap_le_connection_find_by_irk(&request->bonding.irk);
    if (!connection) {
      // The connection may not have an IRK yet: right after pairing, the
      // bonding change handlers (which get here) can run before the driver
      // delivers the IRK update. The connection address has already been
      // updated to the identity address by then, so match by address too.
      PBL_LOG_DBG("register_intent: no IRK match, searching by addr");
      connection = gap_le_connection_by_device(&request->bonding.device);
    }
    if (connection) {
      is_already_connected = true;
      is_already_encrypted = connection->is_encrypted;

      connected_device = &connection->device;
    }
    intent = prv_get_intent_by_bond(&request->bonding.device);
  } else {
    is_already_connected = gap_le_connection_is_connected(request->device);
    intent = prv_get_intent_by_device(request->device);
  }

  if (intent) {
    return PBL_BT_ERRNO_INVALID_STATE;
  }

  // Create intent for device and add to list:
  const size_t alloc_size = sizeof(GAPLEConnectionIntent) +
                            (request->is_bonding_based ? sizeof(GAPLEConnectionIntentBonding) : 0);
  intent = (GAPLEConnectionIntent *)kernel_malloc(alloc_size);
  if (!intent) {
    return PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES;
  }
  memset(intent, 0, alloc_size);
  s_intents = (GAPLEConnectionIntent *)list_prepend(&s_intents->node, &intent->node);

  if (request->is_bonding_based) {
    intent->is_bonding_based = true;
    *intent->bonding = request->bonding;
    if (connected_device) {
      intent->device = *connected_device;
    }
  } else {
    intent->device = *request->device;
    // Append to hardware white-list of BT chip if not connected:
    if (!is_already_connected) {
      prv_mutate_whitelist_safely(request->device, true /* add */);
    }
  }

  intent->auto_reconnect = auto_reconnect;
  intent->is_pairing_required = is_pairing_required;

  if (!is_already_connected) {
    return PBL_BT_ERRNO_OK;
  }

  if (is_pairing_required && !is_already_encrypted) {
    // As slave, the other side is expected to start pairing.
    return PBL_BT_ERRNO_OK;
  }

  // Send the virtual connection event:
  prv_update_intent(intent, PBL_BT_HCI_STATUS_SUCCESS,
                    is_already_encrypted ? GAPLEConnectionEventConnectedAndEncrypted
                                         : GAPLEConnectionEventConnectedNotEncrypted);

  return PBL_BT_ERRNO_OK;
}

// -------------------------------------------------------------------------------------------------
//! Unregisters a connection intent
//! bt_lock() is expected to be taken by the caller
static void prv_unregister_intent(GAPLEConnectionIntent *intent,
                                  bool should_send_disconnection_event, uint8_t hci_reason) {
  const struct pbl_bt_device_internal *device = &intent->device;

  if (gap_le_connection_is_connected(device) && gap_le_connection_is_encrypted(device)) {
    // Disconnect the device because no one is using it
    // If connection is not encrypted, we are likely undergoing a re-pairing,
    // so don't disconnect.
    const int result = pbl_bt_gap_le_disconnect(device);
    if (result != 0) {
      PBL_LOG_ERR("Ble disconnect failed: %d", result);
    }
  } else if (prv_is_intent_using_whitelist(intent)) {
    prv_mutate_whitelist_safely(device, false /* remove */);
  }

  // Only send disconnection event if a connection event has been sent in the past:
  if (should_send_disconnection_event && intent->connected) {
    prv_put_connection_event(device, hci_reason, false /* connected */);
  }

  prv_intent_remove_and_free(intent);
}

// -------------------------------------------------------------------------------------------------

void gap_le_connect_handle_bond_deleted(const struct pbl_bt_device_internal *addr) {
  bt_lock();
  GAPLEConnectionIntent *intent = prv_get_intent_by_bond(addr);
  if (intent) {
    prv_unregister_intent(intent, true /* should_send_disconnection_event */,
                          GAPLEConnectHCIReasonExtensionUserRemovedBonding);
  }
  bt_unlock();
}

// -------------------------------------------------------------------------------------------------

enum pbl_bt_errno gap_le_connect_connect(const struct pbl_bt_device_internal *device,
                                         bool auto_reconnect, bool is_pairing_required) {
  if (!device) {
    return PBL_BT_ERRNO_INVALID_PARAMETER;
  }
  bt_lock();
  struct RegisterIntentRequest request = {
    .is_bonding_based = false,
    .device = device,
  };
  enum pbl_bt_errno ret_value = prv_register_intent(&request, auto_reconnect, is_pairing_required);
  bt_unlock();
  return ret_value;
}

// -------------------------------------------------------------------------------------------------

enum pbl_bt_errno gap_le_connect_cancel(const struct pbl_bt_device_internal *device) {
  if (!device) {
    return PBL_BT_ERRNO_INVALID_PARAMETER;
  }
  bt_lock();
  // Find the intent for the device:
  GAPLEConnectionIntent *intent = prv_get_intent_by_device(device);
  enum pbl_bt_errno ret_value;
  if (!intent) {
    // No intent for given device
    ret_value = PBL_BT_ERRNO_INVALID_PARAMETER;
  } else {
    prv_unregister_intent(intent, true /* should_send_disconnection_event */,
                          GAPLEConnectHCIReasonExtensionCancelConnect);
    ret_value = PBL_BT_ERRNO_OK;
  }
  bt_unlock();
  return ret_value;
}

// -------------------------------------------------------------------------------------------------

enum pbl_bt_errno gap_le_connect_connect_by_bond(const struct pbl_bt_device_internal *addr,
                                                 bool auto_reconnect, bool is_pairing_required) {
  struct pbl_bt_bond_info info;
  if (!addr || pbl_bt_bond_get(addr, &info) != 0) {
    return PBL_BT_ERRNO_INVALID_PARAMETER;
  }
  struct RegisterIntentRequest request = {
    .is_bonding_based = true,
    .bonding = {
      .device = info.addr,
      .irk = info.irk,
    },
  };
  bt_lock();
  enum pbl_bt_errno ret_value = prv_register_intent(&request, auto_reconnect, is_pairing_required);
  bt_unlock();
  return ret_value;
}

// -------------------------------------------------------------------------------------------------

enum pbl_bt_errno gap_le_connect_cancel_by_bond(const struct pbl_bt_device_internal *addr) {
  if (!addr) {
    return PBL_BT_ERRNO_INVALID_PARAMETER;
  }
  bt_lock();
  // Find the intent for the device:
  GAPLEConnectionIntent *intent = prv_get_intent_by_bond(addr);
  enum pbl_bt_errno ret_value;
  if (!intent) {
    // No intent for given device
    ret_value = PBL_BT_ERRNO_INVALID_PARAMETER;
  } else {
    prv_unregister_intent(intent, true /* should_send_disconnection_event */,
                          GAPLEConnectHCIReasonExtensionCancelConnect);
    ret_value = PBL_BT_ERRNO_OK;
  }
  bt_unlock();
  return ret_value;
}

// -------------------------------------------------------------------------------------------------

void gap_le_connect_cancel_all(void) {
  bt_lock();
  {
    PBL_LOG_DBG("Cancel connecting all...");

    GAPLEConnectionIntent *intent = s_intents;
    while (intent) {
      GAPLEConnectionIntent *next = (GAPLEConnectionIntent *)intent->node.next;
      prv_unregister_intent(intent, false /* should_send_disconnection_event */,
                            GAPLEConnectHCIReasonExtensionCancelConnect);
      intent = next;
    }
  }
  bt_unlock();
}

// -------------------------------------------------------------------------------------------------

bool gap_le_connect_is_connected_as_slave(void) {
  bool connected;
  bt_lock();
  {
    connected = s_is_connected_as_slave;
  }
  bt_unlock();
  return connected;
}

// -------------------------------------------------------------------------------------------------

void gap_le_connect_init(void) {
  bt_lock();
  {
    GAPLEConnectionIntent *intent = s_intents;
    while (intent) {
      GAPLEConnectionIntent *next = (GAPLEConnectionIntent *)intent->node.next;
      prv_mutate_whitelist(&intent->device, true /* add */);
      intent = next;
    }

    prv_start_connecting_if_needed();
  }
  bt_unlock();
}

// -------------------------------------------------------------------------------------------------

void gap_le_connect_deinit(void) {
  bt_lock();
  {
    s_has_pending_create_connection = false;

    // Going into air-plane mode, send virtual disconnection events:
    GAPLEConnectionIntent *intent = s_intents;
    while (intent) {
      GAPLEConnectionIntent *next = (GAPLEConnectionIntent *)intent->node.next;
      prv_update_intent(intent, GAPLEConnectHCIReasonExtensionAirPlaneMode,
                        GAPLEConnectionEventDisconnected);
      intent = next;
    }

    if (s_is_connected_as_slave) {
      // The BT controller will not send an etLE_Disconnection_Complete event
      // when going to airplane mode while being connected.
      // Stop analytics stopwatches manually:
      bluetooth_analytics_handle_disconnect(false);
      s_is_connected_as_slave = false;
    }
  }
  bt_unlock();
}

// -------------------------------------------------------------------------------------------------
// For unit testing

bool gap_le_connect_has_pending_create_connection(void) {
  bt_lock();
  const bool ret_value = s_has_pending_create_connection;
  bt_unlock();
  return ret_value;
}

bool gap_le_connect_has_connection_intent(const struct pbl_bt_device_internal *device) {
  bt_lock();
  const bool ret_value = (prv_get_intent_by_device(device) != nullptr);
  bt_unlock();
  return ret_value;
}

bool gap_le_connect_has_connection_intent_for_bond(const struct pbl_bt_device_internal *addr) {
  bt_lock();
  const bool ret_value = (prv_get_intent_by_bond(addr) != nullptr);
  bt_unlock();
  return ret_value;
}

uint32_t gap_le_connect_connection_intents_count(void) {
  bt_lock();
  const uint32_t count = prv_intents_count();
  bt_unlock();
  return count;
}
