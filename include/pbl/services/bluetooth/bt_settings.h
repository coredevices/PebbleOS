/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/bluetooth/types.h>
#include <pbl/services/comm_session/session_remote_version.h>

/**
 * @defgroup services_bluetooth_bt_settings Bluetooth settings
 * @ingroup services_bluetooth
 * @brief Persistent Bluetooth settings of the firmware.
 *
 * Recovery firmware does not persist them: airplane mode is off, the watch is unfaithful and no
 * capabilities are cached. Peer names are kept in shared PRF storage, which only has room for
 * the name of its bond.
 * @{
 */

/** @brief Initialize the settings, before the Bluetooth stack starts. */
void bt_settings_init(void);

/**
 * @brief Get the airplane mode setting.
 *
 * @return True if airplane mode is on; false by default.
 */
bool bt_settings_get_airplane_mode(void);

/**
 * @brief Store the airplane mode setting.
 *
 * @param enable True to turn airplane mode on.
 */
void bt_settings_set_airplane_mode(bool enable);

/**
 * @brief Check whether the watch is unfaithful.
 *
 * The watch becomes unfaithful when it bonds with a new phone, until the next sync.
 *
 * @return True if unfaithful; true by default.
 */
bool bt_settings_is_unfaithful(void);

/**
 * @brief Set whether the watch is unfaithful.
 *
 * @param is_unfaithful New state.
 */
void bt_settings_set_unfaithful(bool is_unfaithful);

/**
 * @brief Get the cached capabilities of the last system session.
 *
 * Prefer comm_session_get_capabilities() when connected.
 *
 * @param[out] capabilities_out Capabilities, zero if none cached.
 */
void bt_settings_get_capabilities(PebbleProtocolCapabilities *capabilities_out);

/**
 * @brief Cache the capabilities of the system session.
 *
 * When they change, they are persisted and a capabilities changed event is emitted.
 *
 * @param capabilities Capabilities, or NULL to clear the cache.
 */
void bt_settings_set_capabilities(const PebbleProtocolCapabilities *capabilities);

/**
 * @brief Get the name of a bonded peer.
 *
 * @param addr Identity address of the peer.
 * @param[out] name Buffer of @c PBL_BT_DEVICE_NAME_BUFFER_SIZE bytes.
 * @return True if a name is stored.
 */
bool bt_settings_get_peer_name(const struct pbl_bt_device_internal *addr,
                               char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]);

/**
 * @brief Store the name of a bonded peer.
 *
 * @param addr Identity address of the peer.
 * @param name Name of the peer.
 */
void bt_settings_set_peer_name(const struct pbl_bt_device_internal *addr, const char *name);

/**
 * @brief Delete the name of a peer.
 *
 * @param addr Identity address of the peer.
 */
void bt_settings_delete_peer_name(const struct pbl_bt_device_internal *addr);

/** @} */
