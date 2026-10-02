/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

#include <stdint.h>

/**
 * @defgroup bluetooth_ppog_reversed Reversed PPoGATT
 * @ingroup bluetooth
 * @brief Reversed PPoGATT service: the watch hosts the service and the phone is the GATT client.
 *
 * The service has a data characteristic the watch notifies and a data write characteristic the
 * phone writes without response, both requiring an encrypted link. The
 * @c pbl_bt_cb_ppog_reversed_* callbacks run on the NimBLE host task without @c bt_lock() held.
 *
 * @code{.c}
 * enum pbl_bt_errno err = pbl_bt_ppog_reversed_notify(conn_handle, pkt, pkt_len);
 * if (err == PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES) {
 *   // No buffer-freed event exists: retry from a timer.
 * }
 * @endcode
 * @{
 */

/**
 * @brief Called when the phone enabled notifications on the data characteristic.
 *
 * @param device The phone.
 * @param conn_handle Connection handle, to pass to pbl_bt_ppog_reversed_notify().
 */
extern void pbl_bt_cb_ppog_reversed_subscribed(const struct pbl_bt_device_internal *device,
                                               uint16_t conn_handle);

/**
 * @brief Called when the phone disabled notifications or disconnected.
 *
 * @param conn_handle Connection handle.
 */
extern void pbl_bt_cb_ppog_reversed_unsubscribed(uint16_t conn_handle);

/**
 * @brief Called when the phone wrote a PPoGATT packet to the data write characteristic.
 *
 * @param conn_handle Connection handle.
 * @param buf The packet, on the kernel heap. Ownership passes to the callee.
 * @param len Length of @p buf in bytes, never 0.
 */
extern void pbl_bt_cb_ppog_reversed_data_written(uint16_t conn_handle, uint8_t *buf, uint16_t len);

/**
 * @brief Send a PPoGATT packet to the phone as a notification.
 *
 * @param conn_handle Connection handle.
 * @param buf The packet. Copied during the call.
 * @param len Length of @p buf in bytes.
 * @retval PBL_BT_ERRNO_OK Sent.
 * @retval PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES Out of buffers. Transient: retry after a short delay,
 *         the stack has no buffers-freed event.
 * @retval PBL_BT_ERRNO_INVALID_STATE Not connected.
 * @return PBL_BT_ERRNO_INTERNAL_ERROR_BEGIN plus the backend error code on other failures.
 */
enum pbl_bt_errno pbl_bt_ppog_reversed_notify(uint16_t conn_handle, const uint8_t *buf,
                                              uint16_t len);

/** @} */
