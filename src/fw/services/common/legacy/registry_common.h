/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "util/attributes.h"

#define MAX_KEY_SIZE_BYTES 16
#define MAX_VALUE_SIZE_BYTES 44
#define UUID_SIZE_BYTES 5

typedef struct PACKED {
  bool active;
  uint8_t key_length;
  uint8_t description;
  uint8_t value_length;
  uint8_t uuid[UUID_SIZE_BYTES];
  char key[MAX_KEY_SIZE_BYTES];
  uint8_t value[MAX_VALUE_SIZE_BYTES];
} Record;

static const uint8_t REGISTRY_SYSTEM_UUID[UUID_SIZE_BYTES] = {
    0x00, 0x00, 0x00, 0x00, 0x00
};
