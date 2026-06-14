/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Maximum number of phones that can be simultaneously connected.
#define MAX_PHONE_CONNECTIONS 2

//! Identifies which phone slot (0 or 1) a connection occupies.
typedef uint8_t PhoneSlot;

#define PHONE_SLOT_INVALID 0xFF
