/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/bluetooth/types.h>

/**
 * @defgroup bluetooth_ppog_reversed Reversed PPoGATT
 * @ingroup bluetooth
 * @brief Reversed PPoGATT service: the watch hosts the service and the phone is the GATT client.
 *
 * The service has a data characteristic the watch notifies and a data write characteristic the
 * phone writes without response, both requiring an encrypted link. The
 * @c pbl_bt_cb_ppog_reversed_* callbacks run on the NimBLE host task.
 *
 * @code{.c}
 * int ret = pbl_bt_ppog_reversed_notify(conn_handle, pkt, pkt_len);
 * if (ret == -ENOMEM) {
 *   // No buffer-freed event exists: retry from a timer.
 * }
 * @endcode
 * @{
 */

/**
 * @brief Called when the phone enabled notifications on the data characteristic.
 *
 * @param conn_handle Connection handle, to pass to pbl_bt_ppog_reversed_notify().
 */
extern void pbl_bt_cb_ppog_reversed_subscribed(uint16_t conn_handle);

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
 * @retval 0 Sent.
 * @retval -ENOMEM Out of buffers. Transient: retry after a short delay, the stack has no
 *         buffers-freed event.
 * @retval -ENOTCONN Not connected.
 * @retval -EIO Other failures.
 */
int pbl_bt_ppog_reversed_notify(uint16_t conn_handle, const uint8_t *buf, uint16_t len);

/** @} */
