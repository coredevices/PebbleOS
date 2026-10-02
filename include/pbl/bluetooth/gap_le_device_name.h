/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gap_le_connection.h"

/**
 * @defgroup bluetooth_gap_le_device_name Peer device names
 * @ingroup bluetooth
 * @brief Read the GAP Device Name of connected peers.
 *
 * The name read is stored in the peer's GAPLEConnection. When it changed,
 * pbl_bt_store_device_name_kernelbg_cb() is scheduled on KernelBG to persist it.
 * @{
 */

/** @brief Request the device name of every connected peer. */
void pbl_bt_gap_le_device_name_request_all(void);
/**
 * @brief Request the device name of a connected peer.
 *
 * The read is queued behind other GATT client procedures.
 *
 * @param address The peer.
 */
void pbl_bt_gap_le_device_name_request(const struct pbl_bt_device_internal *address);

/**
 * @brief Persist the device name of a peer.
 *
 * Implemented by the firmware, run on KernelBG.
 *
 * @param ctx struct pbl_bt_addr of the peer, heap allocated. The callee frees it with
 *            kernel_free().
 */
void pbl_bt_store_device_name_kernelbg_cb(void *ctx);

/** @} */
