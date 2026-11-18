/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/pp/protocol.h>

/**
 * @defgroup services_bluetooth_bt_settings Bluetooth settings
 * @ingroup services_bluetooth
 * @brief Persistent Bluetooth settings of the firmware.
 *
 * Recovery firmware does not persist them: airplane mode is off, the watch is unfaithful and no
 * capabilities are cached.
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
 * Prefer pbl_pp_session_get_capabilities() when connected.
 *
 * @param[out] capabilities_out Capabilities, zero if none cached.
 */
void bt_settings_get_capabilities(struct pbl_pp_capabilities *capabilities_out);

/**
 * @brief Cache the capabilities of the system session.
 *
 * When they change, they are persisted and a capabilities changed event is emitted.
 *
 * @param capabilities Capabilities, or NULL to clear the cache.
 */
void bt_settings_set_capabilities(const struct pbl_pp_capabilities *capabilities);

/** @} */
