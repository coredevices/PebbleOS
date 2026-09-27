/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/gatt_discovery.h>
#include <pbl/bluetooth/hci_types.h>

#include "comm/ble/gap_le_connection.h"

// -- Gatt Device/Server Events

#define PBL_BT_GATT_SERVICE_UUID                        ((uint16_t)0x1801)
#define PBL_BT_GATT_SERVICE_CHANGED_CHARACTERISTIC_UUID ((uint16_t)0x2A05)
#define PBL_BT_GATT_CCCD_UUID                           ((uint16_t)0x2902)

//! Using struct pbl_bt_addr instead of struct pbl_bt_device_internal, with all these events,
//! because Bluetopia's events doesn't contain the address type.

struct pbl_bt_gatt_device_connection_event {
  struct pbl_bt_addr dev_address;
  uint32_t connection_id;
  uint16_t mtu;
};

struct pbl_bt_gatt_device_disconnection_event {
  struct pbl_bt_addr dev_address;
};

struct pbl_bt_gatt_device_buffer_empty_event {
  struct pbl_bt_addr dev_address;
};

struct pbl_bt_gatt_server_notif_indic_event {
  struct pbl_bt_addr dev_address;
  uint16_t attr_handle;
  uint16_t attr_val_len;
  uint8_t *attr_val;
  void *context;
};

struct pbl_bt_gatt_device_mtu_update_event {
  struct pbl_bt_addr dev_address;
  uint16_t mtu;
};

// -- Service Changed Events

struct pbl_bt_gatt_server_changed_confirmation_event {
  struct pbl_bt_addr dev_address;
  uint32_t connection_id;
  uint32_t transaction_id;
  enum pbl_bt_hci_status status_code;
};

struct pbl_bt_gatt_server_read_subscription_event {
  struct pbl_bt_addr dev_address;
  uint32_t connection_id;
  uint32_t transaction_id;
};

struct pbl_bt_gatt_server_subscribe_event {
  struct pbl_bt_addr dev_address;
  uint32_t connection_id;
  bool is_subscribing;
};

// -- Gatt Client Operations

enum pbl_bt_gatt_client_op_response_type {
  PBL_BT_GATT_CLIENT_OP_RESPONSE_READ,
  PBL_BT_GATT_CLIENT_OP_RESPONSE_WRITE,
};

struct pbl_bt_gatt_client_op_response_hdr {
  enum pbl_bt_gatt_client_op_response_type type;
  enum pbl_bt_gatt_error error_code;
  void *context;
};

struct pbl_bt_gatt_client_op_read_response {
  struct pbl_bt_gatt_client_op_response_hdr hdr;
  uint16_t value_length;
  uint8_t *value;
};

struct pbl_bt_gatt_client_op_write_response {
  struct pbl_bt_gatt_client_op_response_hdr hdr;
};

// -- Gatt Data Structures

void pbl_bt_gatt_acknowledge_indication(uint32_t connection_id, uint32_t transaction_id);

// TODO: This will probably need to be changed for the Dialog chip (doesn't have transaction ids)
void pbl_bt_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code);

void pbl_bt_gatt_send_changed_indication(const struct pbl_bt_device_internal *device,
                                         const struct pbl_bt_att_handle_range *data);

enum pbl_bt_errno pbl_bt_gatt_write_without_response(GAPLEConnection *connection,
                                                     const uint8_t *value, size_t value_length,
                                                     uint16_t att_handle);

enum pbl_bt_errno pbl_bt_gatt_write(GAPLEConnection *connection, const uint8_t *value,
                                    size_t value_length, uint16_t att_handle, void *context);

enum pbl_bt_errno pbl_bt_gatt_read(GAPLEConnection *connection, uint16_t att_handle, void *context);

//! The following are callbacks that the backend implementation will call when handling events.

//! gatt callbacks
extern void pbl_bt_cb_gatt_handle_connect(const struct pbl_bt_gatt_device_connection_event *event);

extern void pbl_bt_cb_gatt_handle_disconnect(
    const struct pbl_bt_gatt_device_disconnection_event *event);

extern void pbl_bt_cb_gatt_handle_buffer_empty(
    const struct pbl_bt_gatt_device_buffer_empty_event *event);

extern void pbl_bt_cb_gatt_handle_mtu_update(
    const struct pbl_bt_gatt_device_mtu_update_event *event);

extern void pbl_bt_cb_gatt_handle_notification(
    const struct pbl_bt_gatt_server_notif_indic_event *event);

//! @note The indication is unconditionally confirmed within the backend as soon as one is
//!        received.
extern void pbl_bt_cb_gatt_handle_indication(
    const struct pbl_bt_gatt_server_notif_indic_event *event);

//! gatt_service_changed callbacks
extern void pbl_bt_cb_gatt_service_changed_server_confirmation(
    const struct pbl_bt_gatt_server_changed_confirmation_event *event);

extern void pbl_bt_cb_gatt_service_changed_server_subscribe(
    const struct pbl_bt_gatt_server_subscribe_event *event);

extern void pbl_bt_cb_gatt_service_changed_server_read_subscription(
    const struct pbl_bt_gatt_server_read_subscription_event *event);

extern void pbl_bt_cb_gatt_client_discovery_handle_service_changed(GAPLEConnection *connection,
                                                                   uint16_t handle);

extern void pbl_bt_cb_gatt_client_operations_handle_response(
    struct pbl_bt_gatt_client_op_response_hdr *event);
