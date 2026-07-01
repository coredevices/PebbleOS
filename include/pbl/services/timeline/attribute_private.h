/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/attributes.h"

#include <stdint.h>

typedef struct PACKED {
  uint8_t id;
  uint16_t length;
} SerializedAttributeHeader;
