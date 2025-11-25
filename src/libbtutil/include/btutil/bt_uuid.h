/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <util/uuid.h>

#include <stdint.h>

//! Expands a 16-bit UUID to its 128-bit equivalent, using the Bluetooth base
//! UUID (0000xxxx-0000-1000-8000-00805F9B34FB).
//! @param uuid16 The 16-bit value from which to derive the 128-bit UUID.
//! @return Uuid structure containing the final UUID.
Uuid bt_uuid_expand_16bit(uint16_t uuid16);

//! Expands a 32-bit UUID to its 128-bit equivalent, using the Bluetooth base
//! UUID (xxxxxxxx-0000-1000-8000-00805F9B34FB).
//! @param uuid32 The 32-bit value from which to derive the 128-bit UUID.
//! @return Uuid structure containing the final UUID.
Uuid bt_uuid_expand_32bit(uint32_t uuid32);
