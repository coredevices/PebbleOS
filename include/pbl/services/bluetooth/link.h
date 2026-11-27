/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/bluetooth/responsiveness.h>

#include <nimble/ble.h>

/**
 * @defgroup services_bluetooth_link Link
 * @ingroup services_bluetooth
 * @brief The connection of the watch: there is one at most, to a central.
 *
 * The link becomes the gateway's once it is encrypted and its peer is bonded: only then are the
 * services of the phone used. That, and the loss of it, is reported with
 * @c PEBBLE_BLE_CONNECTION_EVENT; any connection and disconnection with
 * @c PEBBLE_BT_CONNECTION_EVENT.
 * @{
 */

/** @brief Reason of a gateway disconnection that is not HCI's: its bond was removed. */
#define BT_LINK_REASON_BOND_REMOVED 0xFB

/** @brief Start following the link. Call once the stack runs. */
void bt_link_init(void);

/** @brief Drop the link, reporting its gateway as lost. Call before the stack stops. */
void bt_link_deinit(void);

/**
 * @brief Get the connection handle of the link.
 *
 * @return The handle, @c BLE_HS_CONN_HANDLE_NONE if not connected.
 */
uint16_t bt_link_get_conn_handle(void);

/** @brief Tell whether a central is connected. */
bool bt_link_is_connected(void);

/**
 * @brief Tell whether a bonded peer is the one connected.
 *
 * @param identity Identity address of the peer.
 */
bool bt_link_is_connected_to(const ble_addr_t *identity);

/**
 * @brief Read the device name of the peer.
 *
 * The name of a bonded peer is kept with its bond, and @c PEBBLE_BLE_DEVICE_NAME_UPDATED_EVENT is
 * put when it changes.
 */
void bt_link_read_peer_name(void);

/**
 * @brief Copy the device name of the peer, empty until it has been read.
 *
 * @param[out] name The name.
 * @param size Size of @p name.
 */
void bt_link_copy_peer_name(char *name, size_t size);

/** @brief Tell that a bond was added: the link may be the gateway's now. */
void bt_link_handle_bond_added(void);

/** @brief Tell that a bond was deleted: if it was the gateway's, the link is dropped. */
void bt_link_handle_bond_deleted(void);

/** @} */
