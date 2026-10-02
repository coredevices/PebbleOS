/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/uuid.h>

#include <stdint.h>

/**
 * @defgroup btutil_bt_uuid Bluetooth UUIDs
 * @ingroup btutil
 * @brief Expand 16-bit and 32-bit Bluetooth UUIDs to 128 bits.
 *
 * See PBL_BT_SIG_UUID_EXPAND() for the compile-time equivalent.
 * @{
 */

/**
 * @brief Expand a 16-bit UUID with the Bluetooth Base UUID, 0000xxxx-0000-1000-8000-00805F9B34FB.
 *
 * @param uuid16 The 16-bit UUID.
 * @return The 128-bit UUID.
 */
Uuid bt_uuid_expand_16bit(uint16_t uuid16);

/**
 * @brief Expand a 32-bit UUID with the Bluetooth Base UUID, xxxxxxxx-0000-1000-8000-00805F9B34FB.
 *
 * @param uuid32 The 32-bit UUID.
 * @return The 128-bit UUID.
 */
Uuid bt_uuid_expand_32bit(uint32_t uuid32);

/** @} */
