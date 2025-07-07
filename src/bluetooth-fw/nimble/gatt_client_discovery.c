/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/gatt.h>
#include <host/ble_hs.h>
#include <system/logging.h>
#include <util/math.h>

#include "nimble_type_conversions.h"

// -------------------------------------------------------------------------------------------------
// Gatt Client Discovery API calls

typedef struct {
  ListNode node;
  struct ble_gatt_dsc descriptor;
} GATTServiceDiscoveryDescriptorNode;

typedef struct {
  ListNode node;
  struct ble_gatt_chr characteristic;
  ListNode *descriptors;
} GATTServiceDiscoveryCharacteristicNode;

typedef struct {
  ListNode node;
  struct ble_gatt_svc service;
  ListNode *characteristics;
  uint16_t num_descriptors;
} GATTServiceDiscoveryServiceNode;

typedef struct {
  GAPLEConnection *connection;
  ListNode *services;
  ListNode *current_service;
  ListNode *current_characteristic;
} GATTServiceDiscoveryContext;

static bool prv_descriptor_free_cb(ListNode *node, void *context) {
  kernel_free(node);
  return true;
}

static bool prv_characteristic_free_cb(ListNode *node, void *context) {
  GATTServiceDiscoveryCharacteristicNode *chr_node = (GATTServiceDiscoveryCharacteristicNode *)node;
  list_foreach(chr_node->descriptors, prv_descriptor_free_cb, NULL);
  kernel_free(node);
  return true;
}

static bool prv_service_free_cb(ListNode *node, void *context) {
  GATTServiceDiscoveryServiceNode *service_node = (GATTServiceDiscoveryServiceNode *)node;
  list_foreach(service_node->characteristics, prv_characteristic_free_cb, NULL);
  kernel_free(node);
  return true;
}

static void prv_free_discovery_context(GATTServiceDiscoveryContext *context) {
  list_foreach(context->services, prv_service_free_cb, NULL);
  kernel_free(context);
}

/* TODO: the way this works is kinda inefficient, really we should notify the OS after we
 discover all the characteristics/descriptors for a service, letting us free the allocations
 related to that service.
 */
static bool prv_convert_service_and_notify_os_cb(ListNode *node, void *context) {
  GATTServiceDiscoveryServiceNode *service_node = (GATTServiceDiscoveryServiceNode *)node;
  GAPLEConnection *connection = context;

  uint16_t num_characteristics = list_count(service_node->characteristics);
  size_t size_bytes =
      COMPUTE_GATTSERVICE_SIZE_BYTES(num_characteristics, service_node->num_descriptors, 0);
  GATTService *gatt_service = kernel_zalloc_check(size_bytes);
  gatt_service->size_bytes = size_bytes;
  gatt_service->att_handle = service_node->service.start_handle;
  gatt_service->num_characteristics = num_characteristics;
  gatt_service->num_descriptors = service_node->num_descriptors;
  gatt_service->num_att_handles_included_services = 0;
  nimble_uuid_to_pebble(&service_node->service.uuid, &gatt_service->uuid);

  GATTServiceDiscoveryCharacteristicNode *chr_node =
      (GATTServiceDiscoveryCharacteristicNode *)service_node->characteristics;
  uint8_t *end_ptr = (uint8_t *)gatt_service->characteristics;
  while (chr_node != NULL) {
    GATTCharacteristic *gatt_characteristic = (GATTCharacteristic *)end_ptr;
    *gatt_characteristic = (GATTCharacteristic){
        .att_handle_offset = chr_node->characteristic.val_handle - gatt_service->att_handle,
        .properties = chr_node->characteristic.properties,
        .num_descriptors = 0,
    };
    nimble_uuid_to_pebble(&chr_node->characteristic.uuid, &gatt_characteristic->uuid);

    GATTServiceDiscoveryDescriptorNode *dsc_node =
        (GATTServiceDiscoveryDescriptorNode *)chr_node->descriptors;
    uint16_t dsc_index = 0;
    while (dsc_node != NULL) {
      GATTDescriptor *gatt_descriptor = &gatt_characteristic->descriptors[dsc_index];
      *gatt_descriptor = (GATTDescriptor){
          .att_handle_offset = dsc_node->descriptor.handle - gatt_service->att_handle,
      };
      nimble_uuid_to_pebble(&dsc_node->descriptor.uuid, &gatt_descriptor->uuid);

      dsc_index++;
      dsc_node = (GATTServiceDiscoveryDescriptorNode *)list_get_next(&dsc_node->node);
      end_ptr += sizeof(GATTDescriptor);
    }

    gatt_characteristic->num_descriptors = dsc_index;
    end_ptr += sizeof(GATTCharacteristic);
    chr_node = (GATTServiceDiscoveryCharacteristicNode *)list_get_next(&chr_node->node);
  }

  char service_uuid_str[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&gatt_service->uuid, service_uuid_str);
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG,
            "bt_driver_cb_gatt_client_discovery_handle_indication for service %s",
            service_uuid_str);
  bt_driver_cb_gatt_client_discovery_handle_indication(connection, gatt_service, BTErrnoOK);

  return true;
}

static void prv_convert_service_and_notify_os(GATTServiceDiscoveryContext *context) {
  list_foreach(context->services, prv_convert_service_and_notify_os_cb, context->connection);
  bt_driver_cb_gatt_client_discovery_complete(context->connection, BTErrnoOK);
  prv_free_discovery_context(context);
}

static uint16_t prv_get_last_dsc_handle(GATTServiceDiscoveryContext *context) {
  const GATTServiceDiscoveryCharacteristicNode *next_chr =
      (GATTServiceDiscoveryCharacteristicNode *)list_get_next(context->current_characteristic);

  if (next_chr != NULL) {
    return MIN(next_chr->characteristic.def_handle, next_chr->characteristic.val_handle) - 1;
  } else {
    return ((GATTServiceDiscoveryServiceNode *)context->current_service)->service.end_handle;
  }
}

static int prv_find_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg);
static int prv_find_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg);

static void prv_discover_next_dscs(uint16_t conn_handle, GATTServiceDiscoveryContext *context) {
  GATTServiceDiscoveryCharacteristicNode *chr_node =
      (GATTServiceDiscoveryCharacteristicNode *)context->current_characteristic;

  uint16_t start_handle =
      MIN(chr_node->characteristic.val_handle, chr_node->characteristic.def_handle);
  uint16_t end_handle = prv_get_last_dsc_handle(context);
  int rc = ble_gattc_disc_all_dscs(conn_handle, start_handle, end_handle, prv_find_dsc_cb, context);
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "ble_gattc_disc_all_dscs (%d -> %d) rc=0x%04x",
            start_handle, end_handle, (uint16_t)rc);
}

static void prv_discover_next_chrs(uint16_t conn_handle, GATTServiceDiscoveryContext *context) {
  GATTServiceDiscoveryServiceNode *service_node =
      (GATTServiceDiscoveryServiceNode *)context->current_service;
  int rc = ble_gattc_disc_all_chrs(conn_handle, service_node->service.start_handle,
                                   service_node->service.end_handle, prv_find_chr_cb, context);
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "ble_gattc_disc_all_chrs rc=0x%04x", (uint16_t)rc);
}

static GATTServiceDiscoveryDescriptorNode *prv_create_descriptor_node(
    const struct ble_gatt_dsc *dsc) {
  GATTServiceDiscoveryDescriptorNode *dsc_node =
      kernel_zalloc_check(sizeof(GATTServiceDiscoveryDescriptorNode));
  list_init(&dsc_node->node);
  dsc_node->descriptor = *dsc;
  return dsc_node;
}

static GATTServiceDiscoveryCharacteristicNode *prv_create_chr_node(const struct ble_gatt_chr *chr) {
  GATTServiceDiscoveryCharacteristicNode *chr_node =
      kernel_zalloc_check(sizeof(GATTServiceDiscoveryCharacteristicNode));
  list_init(&chr_node->node);
  chr_node->characteristic = *chr;
  return chr_node;
}

static GATTServiceDiscoveryServiceNode *prv_create_service_node(const struct ble_gatt_svc *svc) {
  GATTServiceDiscoveryServiceNode *service_node =
      kernel_zalloc_check(sizeof(GATTServiceDiscoveryServiceNode));
  list_init(&service_node->node);
  service_node->service = *svc;
  return service_node;
}

static GATTServiceDiscoveryCharacteristicNode *prv_get_current_chr(
    GATTServiceDiscoveryContext *context) {
  return (GATTServiceDiscoveryCharacteristicNode *)context->current_characteristic;
}

static GATTServiceDiscoveryServiceNode *prv_get_current_service(
    GATTServiceDiscoveryContext *context) {
  return (GATTServiceDiscoveryServiceNode *)context->current_service;
}

static void prv_list_append_or_set(ListNode **list, ListNode *node) {
  if (*list == NULL) {
    *list = node;
  } else {
    list_append(*list, node);
  }
}

static int prv_find_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg) {
  GATTServiceDiscoveryContext *context = arg;
  BTErrno errno;

  switch (error->status) {
    case 0:
      char chr_uuid_str[BLE_UUID_STR_LEN];
      ble_uuid_to_str(&dsc->uuid.u, chr_uuid_str);
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "discovery: found descriptor %s", chr_uuid_str);

      GATTServiceDiscoveryDescriptorNode *dsc_node = prv_create_descriptor_node(dsc);
      GATTServiceDiscoveryCharacteristicNode *chr_node = prv_get_current_chr(context);

      prv_list_append_or_set(&chr_node->descriptors, &dsc_node->node);

      GATTServiceDiscoveryServiceNode *service_node = prv_get_current_service(context);
      service_node->num_descriptors++;

      break;
    case BLE_HS_EDONE:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "discovery: descriptor discovery done");

      context->current_characteristic = list_get_next(context->current_characteristic);

      if (context->current_characteristic != NULL) {
        prv_discover_next_dscs(conn_handle, context);
      } else {
        context->current_service = list_get_next(context->current_service);
        if (context->current_service != NULL) {
          GATTServiceDiscoveryServiceNode *service_node = prv_get_current_service(context);

          context->current_characteristic = list_get_head(service_node->characteristics);

          prv_discover_next_dscs(conn_handle, context);
        } else {
          // we're done!
          prv_convert_service_and_notify_os(context);
        }
      }

      break;

    default:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "discovery: descriptor discovery error: %d",
                error->status);
      if (error->status == BLE_HS_ETIMEOUT) {
        errno = BTErrnoServiceDiscoveryTimeout;
      } else if (error->status == BLE_HS_ENOTCONN) {
        errno = BTErrnoServiceDiscoveryDisconnected;
      } else {
        errno = BTErrnoInternalErrorBegin + error->status;
      }

      bt_driver_cb_gatt_client_discovery_complete(context->connection, errno);
      prv_free_discovery_context(context);
      break;
  }

  return 0;
}

static int prv_find_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg) {
  GATTServiceDiscoveryContext *context = arg;
  BTErrno errno;

  switch (error->status) {
    case 0:
      char chr_uuid_str[BLE_UUID_STR_LEN];
      ble_uuid_to_str(&chr->uuid.u, chr_uuid_str);
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG,
                "discovery: found characteristic %s (val hdl: %d, def hdl: %d)", chr_uuid_str,
                chr->val_handle, chr->def_handle);

      GATTServiceDiscoveryCharacteristicNode *chr_node = prv_create_chr_node(chr);
      GATTServiceDiscoveryServiceNode *service_node = prv_get_current_service(context);

      prv_list_append_or_set(&service_node->characteristics, &chr_node->node);

      break;

    case BLE_HS_EDONE:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "discovery: characteristic discovery done");

      context->current_service = list_get_next(context->current_service);

      if (context->current_service != NULL) {
        // we have another service to discover characteristics for
        prv_discover_next_chrs(conn_handle, context);
      } else {
        // got all characteristics, now let's get descriptors
        context->current_service = list_get_head(context->services);

        GATTServiceDiscoveryServiceNode *service_node = prv_get_current_service(context);
        context->current_characteristic = list_get_head(service_node->characteristics);

        prv_discover_next_dscs(conn_handle, context);
      }

      break;

    default:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "discovery: characteristic discovery error: %d",
                error->status);
      if (error->status == BLE_HS_ETIMEOUT) {
        errno = BTErrnoServiceDiscoveryTimeout;
      } else if (error->status == BLE_HS_ENOTCONN) {
        errno = BTErrnoServiceDiscoveryDisconnected;
      } else {
        errno = BTErrnoInternalErrorBegin + error->status;
      }

      bt_driver_cb_gatt_client_discovery_complete(context->connection, errno);
      prv_free_discovery_context(context);
      break;
  }

  return 0;
}

static int prv_find_inc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                               const struct ble_gatt_svc *service, void *arg) {
  GATTServiceDiscoveryContext *context = arg;
  BTErrno errno;

  switch (error->status) {
    case 0:
      GATTServiceDiscoveryServiceNode *service_node = prv_create_service_node(service);
      prv_list_append_or_set(&context->services, &service_node->node);

      char service_uuid_str[BLE_UUID_STR_LEN];
      ble_uuid_to_str(&service->uuid.u, service_uuid_str);
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "discovery: found service %s, %d-%d (total %lu)",
                service_uuid_str, service->start_handle, service->end_handle,
                list_count(context->services));
      break;

    case BLE_HS_EDONE:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "service discovery complete");

      if (context->services != NULL) {
        // got services, start discovering characteristics
        context->current_service = list_get_head(context->services);
        prv_discover_next_chrs(conn_handle, context);
      } else {
        // no services found
        bool completed =
            bt_driver_cb_gatt_client_discovery_complete(context->connection, BTErrnoOK);
        PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG,
                  "bt_driver_cb_gatt_client_discovery_complete returned %d", completed);
        prv_free_discovery_context(context);
      }

      break;

    default:
      PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "service discovery error: %d", error->status);
      if (error->status == BLE_HS_ETIMEOUT) {
        errno = BTErrnoServiceDiscoveryTimeout;
      } else if (error->status == BLE_HS_ENOTCONN) {
        errno = BTErrnoServiceDiscoveryDisconnected;
      } else {
        errno = BTErrnoInternalErrorBegin + error->status;
      }

      bt_driver_cb_gatt_client_discovery_complete(context->connection, errno);
      prv_free_discovery_context(context);
      break;
  }
  return 0;
}

BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection,
                                             const ATTHandleRange *data) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "bt_driver_gatt_start_discovery_range %d-%d",
            data->start, data->end);
  uint16_t conn_handle;
  if (!pebble_device_to_nimble_conn_handle(&connection->device, &conn_handle)) {
    return BTErrnoInvalidState;
  }

  GATTServiceDiscoveryContext *context = kernel_zalloc_check(sizeof(GATTServiceDiscoveryContext));
  context->connection = (GAPLEConnection *)connection;

  int rc = ble_gattc_disc_all_svcs(conn_handle, prv_find_inc_svc_cb, (void *)context);
  return rc == 0 ? BTErrnoOK : BTErrnoInternalErrorBegin + rc;
}

// will need to implement this by returning a different value in the callback
// but not sure if this can get called multiple times in parallel, might need
// to stuff the flag in the connection struct
BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) { return 0; }

void bt_driver_gatt_handle_discovery_abandoned(void) {}
