/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel_le_client.h"

#if defined(CONFIG_BT_AMS_CLIENT)
#include <comm/ble/kernel_le_client/ams/ams.h>
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
#include <comm/ble/kernel_le_client/ancs/ancs.h>
#endif
#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/clients/gatt.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/transports/gatt.h>
#include <pbl/services/bluetooth/advertising.h>

#include <comm/ble/gap_le_connection.h>
#include <comm/bt_conn_mgr.h>
#include <comm/bt_lock.h>
#include <kernel/event_loop.h>
#include <kernel/events.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

//! How long the connection is kept fast for the clients to find their services.
#define DISCOVERY_RESPONSE_TIME_SECS 30

// -------------------------------------------------------------------------------------------------
// The Service Changed characteristic of the gateway: it indicates when its services change, and
// the clients then look for theirs again.

static const ble_uuid16_t s_gatt_svc_uuid = BLE_UUID16_INIT(0x1801);
static const ble_uuid16_t s_service_changed_uuid = BLE_UUID16_INIT(0x2A05);
static const ble_uuid_t *const s_gatt_chr_uuids[] = {&s_service_changed_uuid.u};

PBL_BT_GATTC_SERVICE_DEFINE(s_gatt_service, &s_gatt_svc_uuid.u, s_gatt_chr_uuids, 0, 1U << 0);

//! Tells the indications of a gateway that is gone from those of the current one.
static uint32_t s_generation;
static bool s_is_connected;

static void prv_services_changed_kernel_main_cb(void *data) {
  if (!s_is_connected || (uintptr_t)data != s_generation) {
    return;
  }

  PBL_LOG_INFO("Service Changed: looking for the services again");

#if defined(CONFIG_PP_TRANSPORT_GATT)
  pbl_pp_gatt_handle_services_changed();
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
  ancs_handle_services_changed();
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
  ams_handle_services_changed();
#endif
}

// On the host task, as the two below.
static void prv_gatt_service_discovered(struct pbl_bt_gattc_service *svc, int status) {
  if (status == 0) {
    (void)pbl_bt_gattc_service_subscribe(svc, 0, true);
  }
}

static void prv_gatt_service_subscribed(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable,
                                        int status) {
  if (status != 0) {
    PBL_LOG_ERR("Failed to subscribe to service changed: %d", status);
  }
}

static void prv_gatt_service_notified(struct pbl_bt_gattc_service *svc, uint8_t chr,
                                      const uint8_t *data, uint16_t len) {
  // The range of handles affected: whatever it is, each client looks for its service again.
  if (len != 2 * sizeof(uint16_t)) {
    PBL_LOG_ERR("Service Changed Indication incorrect length: %u", len);
    return;
  }

  launcher_task_add_callback(prv_services_changed_kernel_main_cb, (void *)(uintptr_t)s_generation);
}

void kernel_le_client_rediscover(void) {
  launcher_task_add_callback(prv_services_changed_kernel_main_cb, (void *)(uintptr_t)s_generation);
}

static const struct pbl_bt_gattc_service_cb s_gatt_service_cb = {
  .discovered = prv_gatt_service_discovered,
  .subscribed = prv_gatt_service_subscribed,
  .notified = prv_gatt_service_notified,
};

static bool prv_notify_rx(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om) {
  return pbl_bt_gattc_service_notify_rx(&s_gatt_service, conn_handle, attr_handle, om);
}

PBL_BT_GATTC_NOTIFY_LISTENER_DEFINE(gateway, prv_notify_rx);

// -------------------------------------------------------------------------------------------------

static void prv_handle_connection_event(const PebbleBLEConnectionEvent *event) {
  PBL_LOG_DBG("PEBBLE_BLE_CONNECTION_EVENT: reason=0x%x, conn=%u", event->hci_reason,
              event->connected);

  // Assume the gateway is always bonded. The bonds can't be checked here: this
  // may run as a result of a disconnect due to forgetting a bond.

  const struct pbl_bt_device_internal device = PebbleEventToBTDeviceInternal(event);
  if (event->connected) {
    PBL_LOG_DBG("Connected to Gateway!");

    uint16_t conn_handle = 0;
    bool found = false;
    bt_lock();
    GAPLEConnection *connection = gap_le_connection_by_device(&device);
    if (connection) {
      conn_handle = connection->conn_handle;
      found = true;
      conn_mgr_set_ble_conn_response_time(connection, PBL_BT_CONSUMER_LE_SERVICE_DISCOVERY,
                                          PBL_BT_RESPONSE_TIME_MIN, DISCOVERY_RESPONSE_TIME_SECS);
    }
    bt_unlock();
    if (!found) {
      return;
    }

    ++s_generation;
    s_is_connected = true;

    // Each looks for its service, in this order: the session with the phone app comes first.
#if defined(CONFIG_PP_TRANSPORT_GATT)
    pbl_pp_gatt_create();
    pbl_pp_gatt_discover(conn_handle);
#endif
    const int rc = pbl_bt_gattc_service_start(&s_gatt_service, conn_handle, &s_gatt_service_cb);
    if (rc != 0) {
      PBL_LOG_ERR("Could not look for the GATT service (%d)", rc);
    }
#if defined(CONFIG_BT_ANCS_CLIENT)
    ancs_create(conn_handle);
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
    ams_create(conn_handle);
#endif
  } else {
    PBL_LOG_DBG("Disconnected from Gateway!");

    s_is_connected = false;
    pbl_bt_gattc_service_stop(&s_gatt_service);
#if defined(CONFIG_PP_TRANSPORT_GATT)
    pbl_pp_gatt_destroy();
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
    ams_destroy();
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
    ancs_destroy();
#endif
  }
}

// -------------------------------------------------------------------------------------------------
void kernel_le_client_handle_event(const PebbleEvent *e) {
  if (e->type == PEBBLE_BLE_CONNECTION_EVENT) {
    prv_handle_connection_event(&e->bluetooth.le.connection);
  }
}

// -------------------------------------------------------------------------------------------------
static void prv_cleanup_clients_kernel_main_cb(void *unused) {
#if defined(CONFIG_BT_ANCS_CLIENT)
  ancs_destroy();
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
  ams_destroy();
#endif
}

// -------------------------------------------------------------------------------------------------
void kernel_le_client_init(void) {
#if defined(CONFIG_PP_TRANSPORT_GATT)
  pbl_pp_gatt_reset_disconnect_counter();
#endif

  bt_advertising_set_reconnectable(pbl_bt_bond_count() > 0);
}

// -------------------------------------------------------------------------------------------------
void kernel_le_client_deinit(void) {
  // Cleanup clients: their code must execute on KernelMain, so add callback:
  launcher_task_add_callback(prv_cleanup_clients_kernel_main_cb, nullptr);

  pbl_bt_gattc_service_stop(&s_gatt_service);
#if defined(CONFIG_PP_TRANSPORT_GATT)
  pbl_pp_gatt_destroy();
#endif
}
