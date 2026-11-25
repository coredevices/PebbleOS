/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup bluetooth_gap_client GAP client
 * @ingroup bluetooth
 * @brief Client of the Generic Access service of a peer.
 * @{
 */

/**
 * @brief Called with the device name of a peer, on the host task.
 *
 * @param conn_handle Connection.
 * @param name The name, cut to what fits @c PBL_BT_DEVICE_NAME_BUFFER_SIZE; valid during the call.
 */
typedef void (*pbl_bt_gapc_name_cb_t)(uint16_t conn_handle, const char *name);

/**
 * @brief Read the device name of a peer.
 *
 * @param conn_handle Connection.
 * @param cb Called with the name. Not called if it cannot be read.
 */
void pbl_bt_gapc_read_name(uint16_t conn_handle, pbl_bt_gapc_name_cb_t cb);

/** @} */
