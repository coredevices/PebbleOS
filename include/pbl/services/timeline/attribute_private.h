/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <stdint.h>

typedef struct PBL_PACKED {
  uint8_t id;
  uint16_t length;
} SerializedAttributeHeader;
