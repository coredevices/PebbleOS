/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// Convert 4 character strings to values for value based comparison
#define MAKE_BYTE(b) (uint8_t)((b) & 0xFF)
#define MAKE_WORD(a, b, c, d) \
  (uint32_t)((MAKE_BYTE(a) << 24) | (MAKE_BYTE(b) << 16) | (MAKE_BYTE(c) << 8) | MAKE_BYTE(d))
