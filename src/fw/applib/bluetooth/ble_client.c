/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define FILE_LOG_COLOR LOG_COLOR_BLUE

#include "ble_client.h"

#include "ble_app_support.h"

#include "applib/applib_malloc.auto.h"
#include "process_state/app_state/app_state.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#include <stdint.h>

// TODO:
// - device name
// - connection speed (?)
// - app_info.json
// - airplane mode service / BT HW state

// - API to detect that accessory is currently already connected to the phone?
//     - How to identify? iOS SDK does not expose addresses. Use DIS info? Fall
//       back to device name?

static void prv_handle_services_added(
    BLEClientServiceChangeHandler handler, BTDeviceInternal device, BTErrno status) {
  BLEService services[BLE_GATT_MAX_SERVICES_CHANGED];

  uint8_t num_services = sys_ble_client_copy_services(device, services,
                                                      BLE_GATT_MAX_SERVICES_CHANGED);

  if (num_services != 0) {
    handler(device.opaque, BLEClientServicesAdded, services, num_services, status);
  }
}

// TODO (PBL-22086): We should really make this easier to do ...
// We can't directly dereference the service discovery info pointer from third
// party apps because it was malloc'ed from the kernel heap. Instead copy the
// info that is used onto a variable which has been allocated from the stack.
DEFINE_SYSCALL(void, sys_get_service_discovery_info, const PebbleBLEGATTClientServiceEvent *e,
               PebbleBLEGATTClientServiceEventInfo *info) {
  if (PRIVILEGE_WAS_ELEVATED) {
    // Note: if we start storing services, we will need to update the size
    syscall_assert_userspace_buffer(info, sizeof(*info));
  }

  *info = (PebbleBLEGATTClientServiceEventInfo) {
    .type = e->info->type,
    .device = e->info->device,
    .status = e->info->status
  };
}

static void prv_handle_service_change(const PebbleBLEGATTClientEvent *e) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  const BLEClientServiceChangeHandler handler =
                                     ble_app_state->gatt_service_change_handler;
  if (!handler) {
    return;
  }

  PebbleBLEGATTClientServiceEventInfo info;
  sys_get_service_discovery_info((const PebbleBLEGATTClientServiceEvent *)e, &info);

  switch (info.type) {
    case PebbleServicesAdded:
      prv_handle_services_added(handler, info.device, info.status);
      break;
    case PebbleServicesRemoved:
      // TODO (PBL-22087): This is suboptimal. Before we release the BLE API, we should
      // either communicate to the App all the handles which have changed or
      // allow the getters for removed services to still work for the duration
      // of the callback. For now just force a full handle flush and then resync the app
      handler(info.device.opaque, BLEClientServicesInvalidateAll, NULL, 0, info.status);
      prv_handle_services_added(handler, info.device, info.status);
      break;
    case PebbleServicesInvalidateAll:
      handler(info.device.opaque, BLEClientServicesInvalidateAll, NULL, 0, info.status);
      break;
    default:
      WTF;
  }
}

typedef void (*GenericReadHandler)(BLECharacteristic characteristic,
                                   const uint8_t *value,
                                   size_t value_length,
                                   uint16_t value_offset,
                                   BLEGATTError error);

static void prv_consume_read_response(const PebbleBLEGATTClientEvent *e,
                                      GenericReadHandler handler) {
  uint8_t *value = NULL;
  uint16_t value_length = e->value_length;
  const uintptr_t object_ref = e->object_ref;
  BLEGATTError gatt_error = e->gatt_error;

  // Read Responses / Notifications with 0 length data must not be attempted to be consumed
  if (value_length) {
    value = (uint8_t *) applib_malloc(value_length);
    if (!value) {
      gatt_error = BLEGATTErrorLocalInsufficientResources;
      value_length = 0;
    }
    // If there is a read response, we *must* consume it,
    // otherwise the events and associated awaiting responses will
    // get out of sync with each other.
    sys_ble_client_consume_read(object_ref, value, &value_length);
  }

  if (handler) {
    handler(object_ref, value, value_length, 0 /* value_offset (future proofing) */, gatt_error);
  }
  applib_free(value);
}

static void prv_consume_notifications(const PebbleBLEGATTClientEvent *e,
                                      GenericReadHandler handler) {
  uint8_t *value = NULL;
  BLEGATTError gatt_error = e->gatt_error;

  uint16_t heap_buffer_size = 0;
  uint16_t value_length = 0;
  bool has_more = sys_ble_client_get_notification_value_length(&value_length);
  while (has_more) {
    if (heap_buffer_size < value_length) {
      const uint16_t new_heap_buffer_size = MIN(value_length, 64 /* arbitrary min size.. */);
      applib_free(value);
      value = (uint8_t *) applib_malloc(new_heap_buffer_size);
      heap_buffer_size = value ? new_heap_buffer_size : 0;
    }
    if (!value) {
      gatt_error = BLEGATTErrorLocalInsufficientResources;
      value_length = 0;
    }
    uintptr_t object_ref;
    // Consume, even if we didn't have enough memory, this will eat the notification and free up
    // the space in the buffer.
    const uint16_t next_value_length = sys_ble_client_consume_notification(&object_ref,
                                                                           value, &value_length,
                                                                           &has_more);
    if (handler) {
      handler(object_ref, value, value_length, 0 /* value_offset (future proofing) */, gatt_error);
    }

    value_length = next_value_length;
  }

  applib_free(value);
}

static void prv_handle_notifications(const PebbleBLEGATTClientEvent *e) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  prv_consume_notifications(e, ble_app_state->gatt_characteristic_read_handler);
}

static void prv_handle_characteristic_read(const PebbleBLEGATTClientEvent *e) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  prv_consume_read_response(e, ble_app_state->gatt_characteristic_read_handler);
}

static void prv_handle_characteristic_write(const PebbleBLEGATTClientEvent *e) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  const BLEClientWriteHandler handler = ble_app_state->gatt_characteristic_write_handler;
  if (handler) {
    handler(e->object_ref, e->gatt_error);
  }
}

static void prv_handle_characteristic_subscribe(const PebbleBLEGATTClientEvent *e) {
  PBL_LOG(LOG_LEVEL_DEBUG, "TODO: GATT Client Event, subtype=%u", e->subtype);
}

static void prv_handle_descriptor_read(const PebbleBLEGATTClientEvent *e) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  prv_consume_read_response(e, ble_app_state->gatt_descriptor_read_handler);
}

static void prv_handle_descriptor_write(const PebbleBLEGATTClientEvent *e) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  const BLEClientWriteDescriptorHandler handler = ble_app_state->gatt_descriptor_write_handler;
  if (handler) {
    handler(e->object_ref, e->gatt_error);
  }
}

static void prv_handle_buffer_empty(const PebbleBLEGATTClientEvent *e) {
  // TODO
}

typedef void(*PrvHandler)(const PebbleBLEGATTClientEvent *);

static PrvHandler prv_handler_for_subtype(
                                   PebbleBLEGATTClientEventType event_subtype) {
  if (event_subtype >= PebbleBLEGATTClientEventTypeNum) {
    WTF;
  }
  // MT: This is a bit smaller in code size than a switch():
  static const PrvHandler handler[] = {
    [PebbleBLEGATTClientEventTypeServiceChange] = prv_handle_service_change,
    [PebbleBLEGATTClientEventTypeCharacteristicRead] = prv_handle_characteristic_read,
    [PebbleBLEGATTClientEventTypeNotification] = prv_handle_notifications,
    [PebbleBLEGATTClientEventTypeCharacteristicWrite] = prv_handle_characteristic_write,
    [PebbleBLEGATTClientEventTypeCharacteristicSubscribe] = prv_handle_characteristic_subscribe,
    [PebbleBLEGATTClientEventTypeDescriptorRead] = prv_handle_descriptor_read,
    [PebbleBLEGATTClientEventTypeDescriptorWrite] = prv_handle_descriptor_write,
    [PebbleBLEGATTClientEventTypeBufferEmpty] = prv_handle_buffer_empty,
  };
  return handler[event_subtype];
}

// Exported for ble_app_support.c
void ble_client_handle_event(PebbleEvent *e) {
  const PebbleBLEGATTClientEvent *gatt_event = &e->bluetooth.le.gatt_client;
  prv_handler_for_subtype(gatt_event->subtype)(gatt_event);
}

static BTErrno prv_set_handler(void *new_handler, off_t struct_offset_bytes) {
  BLEAppState *ble_app_state = app_state_get_ble_app_state();
  typedef void (*BLEGenericHandler)(void);
  BLEGenericHandler *handler_storage =
       (BLEGenericHandler *)(((uint8_t *) ble_app_state) + struct_offset_bytes);

  const bool had_previous_handler = (*handler_storage == NULL);
  *handler_storage = (BLEGenericHandler) new_handler;

  if (had_previous_handler) {
    if (new_handler) {
      if (ble_app_state->gatt_client_num_handlers++ == 0) {
        // First GATT handler to be registered.
        // Subscribe to GATT Client event service:
        event_service_client_subscribe(&ble_app_state->gatt_client_service_info);
      }
    }
  } else {
    if (!new_handler) {
      if (--ble_app_state->gatt_client_num_handlers == 0) {
        // Last GATT handler to be de-registered.
        // Unsubscribe from GATT Client event service:
        event_service_client_unsubscribe(&ble_app_state->gatt_client_service_info);
      }
    }
  }
  return BTErrnoOK;
}

BTErrno ble_client_set_service_filter(const Uuid service_uuids[],
                                      uint8_t num_uuids) {
  // TODO
  return 0;
}

BTErrno ble_client_set_service_change_handler(BLEClientServiceChangeHandler handler) {
  const off_t offset = offsetof(BLEAppState, gatt_service_change_handler);
  return prv_set_handler(handler, offset);
}

BTErrno ble_client_set_read_handler(BLEClientReadHandler handler) {
  const off_t offset = offsetof(BLEAppState, gatt_characteristic_read_handler);
  return prv_set_handler(handler, offset);
}

BTErrno ble_client_set_write_response_handler(BLEClientWriteHandler handler) {
  const off_t offset = offsetof(BLEAppState, gatt_characteristic_write_handler);
  return prv_set_handler(handler, offset);
}

BTErrno ble_client_set_subscribe_handler(BLEClientSubscribeHandler handler) {
  const off_t offset = offsetof(BLEAppState, gatt_characteristic_subscribe_handler);
  return prv_set_handler(handler, offset);
}

BTErrno ble_client_set_buffer_empty_handler(BLEClientBufferEmptyHandler empty_handler) {
  // TODO
  return BTErrnoOther;
}

BTErrno ble_client_set_descriptor_write_handler(BLEClientWriteDescriptorHandler handler) {
  const off_t offset = offsetof(BLEAppState, gatt_descriptor_write_handler);
  return prv_set_handler(handler, offset);
}

BTErrno ble_client_set_descriptor_read_handler(BLEClientReadDescriptorHandler handler) {
  const off_t offset = offsetof(BLEAppState, gatt_descriptor_read_handler);
  return prv_set_handler(handler, offset);
}
