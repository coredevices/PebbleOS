/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/hci_types.h>
#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>

#include <comm/ble/gap_le_connection.h>

/**
 * @defgroup bluetooth_gatt GATT
 * @ingroup bluetooth
 * @brief GATT events and the Service Changed characteristic.
 *
 * The events identify the peer by struct pbl_bt_addr: the address type is not available.
 * @{
 */

/** @brief 16-bit UUID of the Generic Attribute service. */
#define PBL_BT_GATT_SERVICE_UUID ((uint16_t)0x1801)
/** @brief 16-bit UUID of the Service Changed characteristic. */
#define PBL_BT_GATT_SERVICE_CHANGED_CHARACTERISTIC_UUID ((uint16_t)0x2A05)
/** @brief 16-bit UUID of the Client Characteristic Configuration Descriptor. */
#define PBL_BT_GATT_CCCD_UUID ((uint16_t)0x2902)

/** @brief A GATT connection was established. Not reported by the NimBLE backend. */
struct pbl_bt_gatt_device_connection_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
  /** Backend specific connection identifier. */
  uint32_t connection_id;
  /** ATT MTU. */
  uint16_t mtu;
};

/** @brief A GATT connection was terminated. */
struct pbl_bt_gatt_device_disconnection_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
};

/** @brief The ATT MTU of a connection changed. */
struct pbl_bt_gatt_device_mtu_update_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
  /** New ATT MTU. */
  uint16_t mtu;
};

/** @brief A peer confirmed a Service Changed indication. Not reported by NimBLE. */
struct pbl_bt_gatt_server_changed_confirmation_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
  /** Backend specific connection identifier. */
  uint32_t connection_id;
  /** Backend specific transaction identifier. */
  uint32_t transaction_id;
  /** Status of the confirmation. */
  enum pbl_bt_hci_status status_code;
};

/** @brief A peer read the Service Changed CCCD. Not reported by NimBLE. */
struct pbl_bt_gatt_server_read_subscription_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
  /** Backend specific connection identifier. */
  uint32_t connection_id;
  /** Transaction to answer with pbl_bt_gatt_respond_read_subscription(). */
  uint32_t transaction_id;
};

/** @brief A peer (un)subscribed to Service Changed indications. Not reported by NimBLE. */
struct pbl_bt_gatt_server_subscribe_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
  /** Backend specific connection identifier. */
  uint32_t connection_id;
  /** True when subscribing, false when unsubscribing. */
  bool is_subscribing;
};

/** @brief Inclusive range of ATT handles. */
struct PBL_PACKED pbl_bt_att_handle_range {
  /** First handle. */
  uint16_t start;
  /** Last handle. */
  uint16_t end;
};

/**
 * @brief Answer a read of the Service Changed CCCD.
 *
 * Does nothing in the NimBLE backend.
 *
 * @param transaction_id Transaction from struct pbl_bt_gatt_server_read_subscription_event.
 * @param response_code CCCD value to return.
 */
void pbl_bt_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code);

/**
 * @brief Indicate a change of the local GATT database to a peer.
 *
 * Sends a Service Changed indication with the affected handle range.
 *
 * @param device The peer.
 * @param data Changed handle range.
 */
void pbl_bt_gatt_send_changed_indication(const struct pbl_bt_device_internal *device,
                                         const struct pbl_bt_att_handle_range *data);

/**
 * @brief Called when a GATT connection is established. Not invoked by the NimBLE backend.
 *
 * @param event The connection.
 */
extern void pbl_bt_cb_gatt_handle_connect(const struct pbl_bt_gatt_device_connection_event *event);

/**
 * @brief Called when a GATT connection is terminated.
 *
 * Invoked before pbl_bt_handle_le_disconnection_complete_event().
 *
 * @param event The disconnection.
 */
extern void pbl_bt_cb_gatt_handle_disconnect(
    const struct pbl_bt_gatt_device_disconnection_event *event);

/**
 * @brief Called when the ATT MTU of a connection changed.
 *
 * @param event The new MTU.
 */
extern void pbl_bt_cb_gatt_handle_mtu_update(
    const struct pbl_bt_gatt_device_mtu_update_event *event);

/**
 * @brief Called when a peer confirmed a Service Changed indication. Not invoked by NimBLE.
 *
 * @param event The confirmation.
 */
extern void pbl_bt_cb_gatt_service_changed_server_confirmation(
    const struct pbl_bt_gatt_server_changed_confirmation_event *event);

/**
 * @brief Called when a peer (un)subscribed to Service Changed indications. Not invoked by NimBLE.
 *
 * @param event The subscription change.
 */
extern void pbl_bt_cb_gatt_service_changed_server_subscribe(
    const struct pbl_bt_gatt_server_subscribe_event *event);

/**
 * @brief Called when a peer read the Service Changed CCCD. Not invoked by NimBLE.
 *
 * Answer with pbl_bt_gatt_respond_read_subscription().
 *
 * @param event The read.
 */
extern void pbl_bt_cb_gatt_service_changed_server_read_subscription(
    const struct pbl_bt_gatt_server_read_subscription_event *event);

/** @} */
