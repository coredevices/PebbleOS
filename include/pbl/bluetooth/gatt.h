/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/gatt_discovery.h>
#include <pbl/bluetooth/hci_types.h>

#include "comm/ble/gap_le_connection.h"

/**
 * @defgroup bluetooth_gatt GATT
 * @ingroup bluetooth
 * @brief GATT client operations, GATT events and the Service Changed characteristic.
 *
 * Reads and writes complete asynchronously: the result is delivered to
 * pbl_bt_cb_gatt_client_operations_handle_response() on the NimBLE host task, together with the
 * context passed when issuing the operation.
 *
 * @code{.c}
 * enum pbl_bt_errno err = pbl_bt_gatt_read(connection, value_handle, my_ctx);
 * if (err != PBL_BT_ERRNO_OK) {
 *   // Not issued, no response will follow.
 * }
 *
 * static const uint8_t enable[] = { 0x01, 0x00 };
 * pbl_bt_gatt_write(connection, enable, sizeof(enable), cccd_handle, my_ctx);
 *
 * void pbl_bt_cb_gatt_client_operations_handle_response(
 *     struct pbl_bt_gatt_client_op_response_hdr *event) {
 *   if (event->type == PBL_BT_GATT_CLIENT_OP_RESPONSE_READ &&
 *       event->error_code == PBL_BT_GATT_ERROR_SUCCESS) {
 *     const struct pbl_bt_gatt_client_op_read_response *rsp = (const void *)event;
 *     // rsp->value is only valid during the callback.
 *     prv_handle_value(event->context, rsp->value, rsp->value_length);
 *   }
 * }
 * @endcode
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

/** @brief The transmit buffers to a peer became available again. Not reported by NimBLE. */
struct pbl_bt_gatt_device_buffer_empty_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
};

/** @brief A notification or indication was received from a peer's GATT server. */
struct pbl_bt_gatt_server_notif_indic_event {
  /** Address of the peer. */
  struct pbl_bt_addr dev_address;
  /** Handle of the attribute. */
  uint16_t attr_handle;
  /** Length of @ref attr_val in bytes. */
  uint16_t attr_val_len;
  /** Attribute value. Only valid during the callback. */
  uint8_t *attr_val;
  /** Unused by the NimBLE backend. */
  void *context;
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

/** @brief Type of a GATT client operation response. */
enum pbl_bt_gatt_client_op_response_type {
  /** Response to pbl_bt_gatt_read(), a struct pbl_bt_gatt_client_op_read_response. */
  PBL_BT_GATT_CLIENT_OP_RESPONSE_READ,
  /** Response to pbl_bt_gatt_write(), a struct pbl_bt_gatt_client_op_write_response. */
  PBL_BT_GATT_CLIENT_OP_RESPONSE_WRITE,
};

/** @brief Common header of GATT client operation responses. */
struct pbl_bt_gatt_client_op_response_hdr {
  /** Response type, tells which response structure embeds this header. */
  enum pbl_bt_gatt_client_op_response_type type;
  /**
   * Result. ATT errors are reported as is; other failures as PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN
   * plus the backend error code.
   */
  enum pbl_bt_gatt_error error_code;
  /** Context passed to pbl_bt_gatt_read() or pbl_bt_gatt_write(). */
  void *context;
};

/** @brief Response to pbl_bt_gatt_read(). */
struct pbl_bt_gatt_client_op_read_response {
  /** Common header. */
  struct pbl_bt_gatt_client_op_response_hdr hdr;
  /** Length of @ref value in bytes. */
  uint16_t value_length;
  /** Value read. Only valid during the callback. */
  uint8_t *value;
};

/** @brief Response to pbl_bt_gatt_write(). */
struct pbl_bt_gatt_client_op_write_response {
  /** Common header. */
  struct pbl_bt_gatt_client_op_response_hdr hdr;
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
 * @brief Write a characteristic value without response.
 *
 * @param connection The connection.
 * @param value Value to write. Copied during the call.
 * @param value_length Length of @p value in bytes, at most ATT MTU - 3.
 * @param att_handle Handle of the attribute.
 * @retval PBL_BT_ERRNO_OK The write was queued.
 * @retval PBL_BT_ERRNO_INVALID_STATE @p connection is not connected.
 * @return PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN plus the backend error code on other failures.
 */
enum pbl_bt_errno pbl_bt_gatt_write_without_response(GAPLEConnection *connection,
                                                     const uint8_t *value, size_t value_length,
                                                     uint16_t att_handle);

/**
 * @brief Write an attribute value.
 *
 * The result is reported through pbl_bt_cb_gatt_client_operations_handle_response().
 *
 * @param connection The connection.
 * @param value Value to write. Copied during the call.
 * @param value_length Length of @p value in bytes.
 * @param att_handle Handle of the attribute.
 * @param context Passed back in the response.
 * @retval PBL_BT_ERRNO_OK The write was issued.
 * @retval PBL_BT_ERRNO_INVALID_STATE @p connection is not connected.
 * @return PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN plus the backend error code on other failures. No
 *         response follows a failure.
 */
enum pbl_bt_errno pbl_bt_gatt_write(GAPLEConnection *connection, const uint8_t *value,
                                    size_t value_length, uint16_t att_handle, void *context);

/**
 * @brief Read an attribute value.
 *
 * The value is reported through pbl_bt_cb_gatt_client_operations_handle_response().
 *
 * @param connection The connection.
 * @param att_handle Handle of the attribute.
 * @param context Passed back in the response.
 * @retval PBL_BT_ERRNO_OK The read was issued.
 * @retval PBL_BT_ERRNO_INVALID_STATE @p connection is not connected.
 * @return PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN plus the backend error code on other failures. No
 *         response follows a failure.
 */
enum pbl_bt_errno pbl_bt_gatt_read(GAPLEConnection *connection, uint16_t att_handle, void *context);

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
 * @brief Called when the transmit buffers became available again. Not invoked by NimBLE.
 *
 * @param event The peer.
 */
extern void pbl_bt_cb_gatt_handle_buffer_empty(
    const struct pbl_bt_gatt_device_buffer_empty_event *event);

/**
 * @brief Called when the ATT MTU of a connection changed.
 *
 * @param event The new MTU.
 */
extern void pbl_bt_cb_gatt_handle_mtu_update(
    const struct pbl_bt_gatt_device_mtu_update_event *event);

/**
 * @brief Called when a notification is received.
 *
 * @param event The notification.
 */
extern void pbl_bt_cb_gatt_handle_notification(
    const struct pbl_bt_gatt_server_notif_indic_event *event);

/**
 * @brief Called when an indication is received.
 *
 * The backend confirms the indication on reception, before this is called.
 *
 * @param event The indication.
 */
extern void pbl_bt_cb_gatt_handle_indication(
    const struct pbl_bt_gatt_server_notif_indic_event *event);

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

/**
 * @brief Called after subscribing to a peer's Service Changed indications.
 *
 * @param connection The connection.
 * @param handle Value handle of the peer's Service Changed characteristic.
 */
extern void pbl_bt_cb_gatt_client_discovery_handle_service_changed(GAPLEConnection *connection,
                                                                   uint16_t handle);

/**
 * @brief Called with the result of pbl_bt_gatt_read() or pbl_bt_gatt_write().
 *
 * @param event Response header. Cast to the structure given by its @c type.
 */
extern void pbl_bt_cb_gatt_client_operations_handle_response(
    struct pbl_bt_gatt_client_op_response_hdr *event);

/** @} */
