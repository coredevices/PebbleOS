/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel_le_client.h"

#if defined(CONFIG_BT_ANCS_CLIENT)
#include <comm/ble/kernel_le_client/ancs/ancs.h>
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
#include <comm/ble/kernel_le_client/ams/ams.h>
#endif
#if UNITTEST
#include <comm/ble/kernel_le_client/test/test_definition.h>
#endif

#include <pbl/bluetooth/bond.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/transports/gatt.h>
#include <pbl/services/bluetooth/advertising.h>

#include <comm/ble/gap_le_connection.h>
#include <comm/ble/gatt_client_accessors.h>
#include <comm/ble/gatt_client_discovery.h>
#include <comm/ble/gatt_client_operations.h>
#include <comm/ble/gatt_client_subscriptions.h>
#include <comm/bt_lock.h>
#include <kernel/event_loop.h>
#include <kernel/events.h>

#define MAX_SERVICE_INSTANCES (8)

#define NOTIFICATION_MIN_FREE_KERNEL_EVENTS (4)

//! Array indices for the different client "classes"
enum {
#if UNITTEST
  KernelLEClientUnitTest = 0,
#else
#if defined(CONFIG_PP_TRANSPORT_GATT)
  KernelLEClientPPoGATT,
#endif
#endif
  KernelLEClientNum,
};

typedef struct {
  //! Name of the GATT profile that will be used in debug logs
  const char *const debug_name;
  //! The Service UUID of the remote GATT service
  const Uuid *const service_uuid;
  //! Array of Characteristic UUIDs that are expected to be part of the remote GATT service
  const Uuid *const characteristic_uuids;
  //! The number of elements in the `characteristic_uuids` array
  const uint8_t num_characteristics;
  //! Callback executed every time a BT LE service matching 'service_uuid' is discovered
  //!
  //! @param characteristics - handles for the characteristics discovered.
  //!    The array will be 'num_characteristics' size and ordered the same way
  //!    as the characteristics_uuids array provided
  void (*handle_service_discovered)(pbl_bt_characteristic_t *characteristics);
  //! Callback executed every time a BT LE service matching 'service_uuid' is removed
  //!
  //!  @param characteristics - An array of all the characteristic handles that
  //!    have been invalidated
  //!  @param num_characteristics - The length of the array
  void (*handle_service_removed)(pbl_bt_characteristic_t *characteristics,
                                 uint8_t num_characteristics);
  //! Invoked when all handles should be flushed by the connection
  //! (events such as a disconnect or full re-discovery will trigger this)
  void (*invalidate_all_references)(void);
  //! Function that is called to test whether the client handles the characteristic, in which case
  //! write/read responses/notifications will be routed to this client (can be NULL)
  bool (*can_handle_characteristic)(pbl_bt_characteristic_t characteristic);
  //! Handler for GATT read responses and notifications / indications (can be NULL)
  void (*handle_read_or_notification)(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                                      size_t value_length, enum pbl_bt_gatt_error error);
  //! Handler for GATT write responses (can be NULL)
  void (*handle_write_response)(pbl_bt_characteristic_t characteristic,
                                enum pbl_bt_gatt_error error);
  //! Handler for GATT subscription confirmations (can be NULL)
  void (*handle_subscribe)(pbl_bt_characteristic_t subscribed_characteristic,
                           BLESubscription subscription_type, enum pbl_bt_gatt_error error);
} KernelLEClient;

static const KernelLEClient s_clients[KernelLEClientNum] = {
#if UNITTEST
  [KernelLEClientUnitTest] = {
    .debug_name = "TEST",
    .service_uuid = &s_test_service_uuid,
    .characteristic_uuids = s_test_characteristic_uuids,
    .num_characteristics = TestCharacteristicCount,
    .handle_service_discovered = test_client_handle_service_discovered,
    .handle_service_removed = test_client_handle_service_removed,
    .invalidate_all_references = test_client_invalidate_all_references,
    .can_handle_characteristic = test_client_can_handle_characteristic,
    .handle_write_response = test_client_handle_write_response,
    .handle_subscribe = test_client_handle_subscribe,
    .handle_read_or_notification = test_client_handle_read_or_notification,
  },
#else
#if defined(CONFIG_PP_TRANSPORT_GATT)
  [KernelLEClientPPoGATT] = {
    .debug_name = "PPoG",
    .service_uuid = &pbl_pp_gatt_service_uuid,
    .characteristic_uuids = pbl_pp_gatt_characteristic_uuids,
    .num_characteristics = PBL_PP_GATT_CHARACTERISTIC_NUM,
    .handle_service_discovered = pbl_pp_gatt_handle_service_discovered,
    .handle_service_removed = pbl_pp_gatt_handle_service_removed,
    .invalidate_all_references = pbl_pp_gatt_invalidate_all_references,
    .can_handle_characteristic = pbl_pp_gatt_can_handle_characteristic,
    .handle_write_response = nullptr,
    .handle_subscribe = pbl_pp_gatt_handle_subscribe,
    .handle_read_or_notification = pbl_pp_gatt_handle_read_or_notification,
  },
#endif
#endif // UNITTEST
};

static void prv_handle_services_removed(PebbleBLEGATTClientServicesRemoved *services_removed) {
  PebbleBLEGATTClientServiceHandles *service_remove_info = &services_removed->handles[0];
  for (int s = 0; s < services_removed->num_services_removed; s++) {
    for (int c = 0; c < KernelLEClientNum; c++) {
      const KernelLEClient *const client = &s_clients[c];
      if (uuid_equal(&service_remove_info->uuid, client->service_uuid)) {
        client->handle_service_removed(
            (pbl_bt_characteristic_t *)&service_remove_info->char_and_desc_handles[0],
            service_remove_info->num_characteristics);
      }
    }

    int num_hdls = service_remove_info->num_descriptors + service_remove_info->num_characteristics;
    service_remove_info =
        (PebbleBLEGATTClientServiceHandles *)&service_remove_info->char_and_desc_handles[num_hdls];
  }
}

static void prv_handle_all_services_invalidated(void) {
  for (int c = 0; c < KernelLEClientNum; c++) {
    const KernelLEClient *const client = &s_clients[c];
    client->invalidate_all_references();
  }
}

static void prv_handle_services_added(PebbleBLEGATTClientServicesAdded *added_services,
                                      struct pbl_bt_device_internal *device) {
  // loop through the new services
  for (int s = 0; s < added_services->num_services_added; s++) {
    // get the uuid for the service
    Uuid service_uuid = gatt_client_service_get_uuid(added_services->services[s]);

    // are any clients looking for this uuid?
    for (int c = 0; c < KernelLEClientNum; c++) {
      const KernelLEClient *const client = &s_clients[c];

      if (!uuid_equal(&service_uuid, (const Uuid *)client->service_uuid)) {
        continue;
      }

      // We have found a service that a client is looking for. Make sure the
      // characteristics we want are present and if so notify the interested client about it
      pbl_bt_characteristic_t characteristics[client->num_characteristics];
      const uint8_t num_characteristics = gatt_client_service_get_characteristics_matching_uuids(
          added_services->services[s], &characteristics[0], client->characteristic_uuids,
          client->num_characteristics);

      if (num_characteristics != client->num_characteristics) {
        PBL_LOG_ERR("Found %s, but only %u characteristics...", client->debug_name,
                    num_characteristics);
        continue;
      }

      client->handle_service_discovered(characteristics);
    }
  }
}

// Clients that find their service themselves look for it again: it may be among the added.
static void prv_handle_services_changed(void) {
#if defined(CONFIG_BT_ANCS_CLIENT)
  ancs_handle_services_changed();
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
  ams_handle_services_changed();
#endif
}

static void prv_handle_gatt_service_discovery_event(const PebbleBLEGATTClientServiceEvent *event) {
  PebbleBLEGATTClientServiceEventInfo *event_info = event->info;
  if (event_info->status == PBL_BT_ERRNO_SERVICE_DISCOVERY_DISCONNECTED) {
    // TODO: In the past we'd disconnect when service discovery
    // failed (not due to a disconnection)
    return;
  }
  if (event_info->status != PBL_BT_ERRNO_SERVICE_DISCOVERY_DATABASE_CHANGED &&
      event_info->status != PBL_BT_ERRNO_OK) {
    // gatt_client_discovery.c already logs errors for this condition
    return;
  }

  if (event_info->type != PebbleServicesRemoved) {
    // For removals, we log info in the handler routine
    PBL_LOG_INFO("Service changed Indication: type: %d status: %d", event_info->type,
                 event_info->status);
  }

  switch (event_info->type) {
    case PebbleServicesRemoved:
      prv_handle_services_removed(&event_info->services_removed_data);
      prv_handle_services_changed();
      break;
    case PebbleServicesInvalidateAll:
      prv_handle_all_services_invalidated();
      prv_handle_services_changed();
      break;
    case PebbleServicesAdded:
      prv_handle_services_added(&event_info->services_added_data, &event_info->device);
      prv_handle_services_changed();
      break;
    default:
      pbl_panic();
  }
}

static const KernelLEClient *prv_client_for_characteristic(pbl_bt_characteristic_t characteristic) {
  for (int c = 0; c < KernelLEClientNum; ++c) {
    const KernelLEClient *const client = &s_clients[c];
    if (client->can_handle_characteristic && client->can_handle_characteristic(characteristic)) {
      return client;
    }
  }
  return nullptr;
}

typedef void (*ReadNotifyHandler)(pbl_bt_characteristic_t characteristic, const uint8_t *value,
                                  size_t value_length, enum pbl_bt_gatt_error error);

static void prv_consume_read_response(const PebbleBLEGATTClientEvent *event,
                                      const KernelLEClient *client) {
  const uint16_t value_length = event->value_length;
  uint8_t *buffer = nullptr;

  if (value_length) {
    // TODO: avoid double-copying the data
    buffer = (uint8_t *)pbl_malloc(value_length);
    if (PBL_UNLIKELY(!buffer)) {
      PBL_LOG_ERR("OOM for GATT read response - %d bytes", (int)value_length);
      return;
    }
    PBL_ASSERTN(gatt_client_consume_read_response(event->object_ref, buffer, value_length));
  }

  if (client->handle_read_or_notification) {
    client->handle_read_or_notification(event->object_ref, buffer, value_length, event->gatt_error);
  }
  pbl_free(buffer);
}

static void prv_consume_notifications(const PebbleBLEGATTClientEvent *event) {
  GATTBufferedNotificationHeader header = {};
  bool has_more = gatt_client_subscriptions_get_notification_header(&header);
  const uint64_t start_ticks = pbl_uptime_ticks();
  while (has_more) {
    const uint32_t ticks_spent = pbl_uptime_ticks() - start_ticks;

    // Don't spend more than ~33ms (or one 30fps animation frame interval) processing the pending
    // GATT notifications, and let KernelMain drain the events posted by the handlers:
    if ((ticks_spent >= ((PBL_TICK_HZ * 33) / 1000)) ||
        (event_kernel_to_kernel_num_free() < NOTIFICATION_MIN_FREE_KERNEL_EVENTS)) {
      // Doing this might actually cause an issue if the characteristic(s) for which there are still
      // notifications pending in the buffer become invalid before the time they are processed.
      // Probably not a big deal.
      gatt_client_subscriptions_reschedule();
      return; // yield
    }

    // TODO: avoid double-copying the data
    uint8_t *buffer = (uint8_t *)pbl_malloc(header.value_length);
    if (PBL_UNLIKELY(header.value_length && !buffer)) {
      PBL_LOG_ERR("OOM for GATT notification");
      return;
    }

    pbl_bt_characteristic_t characteristic;
    uint16_t value_length = header.value_length;
    const uint16_t next_value_length = gatt_client_subscriptions_consume_notification(
        &characteristic, buffer, &value_length, &has_more);
    header.characteristic = characteristic;
    header.value_length = value_length;

    const KernelLEClient *const client = prv_client_for_characteristic(header.characteristic);
    if (client->handle_read_or_notification) {
      client->handle_read_or_notification(header.characteristic, buffer, header.value_length,
                                          PBL_BT_GATT_ERROR_SUCCESS);
    } else {
      PBL_LOG_DBG("No client to handle GATT notification from characteristic %p",
                  (void *)header.characteristic);
    }
    pbl_free(buffer);
    header.value_length = next_value_length;
  }
}

static void prv_handle_gatt_event(const PebbleBLEGATTClientEvent *event) {
  if (event->subtype == PebbleBLEGATTClientEventTypeNotification) {
    prv_consume_notifications(event);
    return;
  }

  const KernelLEClient *const client = prv_client_for_characteristic(event->object_ref);
  if (!client) {
    // Read responses still need to be consumed, even if the client has disappeared:
    if (event->subtype == PebbleBLEGATTClientEventTypeCharacteristicRead && event->value_length) {
      PBL_ASSERTN(
          gatt_client_consume_read_response(event->object_ref, nullptr, event->value_length));
    }
    goto log_error;
  }

  switch (event->subtype) {
    case PebbleBLEGATTClientEventTypeCharacteristicWrite:
      if (!client->handle_write_response) {
        goto log_error;
      }
      client->handle_write_response(event->object_ref, event->gatt_error);
      return;

    case PebbleBLEGATTClientEventTypeCharacteristicSubscribe:
      if (!client->handle_subscribe) {
        goto log_error;
      }
      client->handle_subscribe(event->object_ref, event->subscription_type, event->gatt_error);
      return;

    case PebbleBLEGATTClientEventTypeCharacteristicRead:
      if (!client->handle_read_or_notification) {
        goto log_error;
      }
      prv_consume_read_response(event, client);
      return;

    default:
      break;
  }

log_error:
  PBL_LOG_ERR("Unhandled GATT event:%u ref:%" PRIu32 " err:%" PRIu16 " len:%" PRIu16 " cl:%p",
              event->subtype, (uint32_t)event->object_ref, (uint16_t)event->gatt_error,
              (uint16_t)event->value_length, client);
}

static void prv_handle_connection_event(const PebbleBLEConnectionEvent *event) {
  PBL_LOG_DBG("PEBBLE_BLE_CONNECTION_EVENT: reason=0x%x, conn=%u", event->hci_reason,
              event->connected);

  const bool connected = event->connected;
  // Assume the Kernel LE client is always bonded for ANCS. The bonds can't be checked here: this
  // may run as a result of a disconnect due to forgetting a bond.

  const struct pbl_bt_device_internal device = PebbleEventToBTDeviceInternal(event);
  if (connected) {
    PBL_LOG_DBG("Connected to Gateway!");

#if defined(CONFIG_PP_TRANSPORT_GATT)
    pbl_pp_gatt_create();
#endif

    // First, so that the clients that look for their service themselves do not hold it up.
    gatt_client_discovery_discover_all(&device);

#if defined(CONFIG_BT_ANCS_CLIENT) || defined(CONFIG_BT_AMS_CLIENT)
    uint16_t conn_handle = 0;
    bool found = false;
    bt_lock();
    const GAPLEConnection *connection = gap_le_connection_by_device(&device);
    if (connection) {
      conn_handle = connection->conn_handle;
      found = true;
    }
    bt_unlock();
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
    if (found) {
      ancs_create(conn_handle);
    }
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
    if (found) {
      ams_create(conn_handle);
    }
#endif
  } else {
    PBL_LOG_DBG("Disconnected from Gateway!");
#if defined(CONFIG_PP_TRANSPORT_GATT)
    pbl_pp_gatt_destroy();
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
    ams_destroy();
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
    ancs_destroy();
#endif
    gatt_client_op_cleanup();
  }
}

// -------------------------------------------------------------------------------------------------
void kernel_le_client_handle_event(const PebbleEvent *e) {
  switch (e->type) {
    case PEBBLE_BLE_CONNECTION_EVENT:
      prv_handle_connection_event(&e->bluetooth.le.connection);
      return;

    case PEBBLE_BLE_GATT_CLIENT_EVENT:
      if (e->bluetooth.le.gatt_client.subtype == PebbleBLEGATTClientEventTypeServiceChange) {
        prv_handle_gatt_service_discovery_event(&e->bluetooth.le.gatt_client_service);
      } else {
        prv_handle_gatt_event(&e->bluetooth.le.gatt_client);
      }
      return;

    default:
      return;
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

#if defined(CONFIG_PP_TRANSPORT_GATT)
  pbl_pp_gatt_destroy();
#endif
}
