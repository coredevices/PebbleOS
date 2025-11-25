/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/uuid.h"
#include "util/rand32.h"

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const Uuid system_uuid = UUID_SYSTEM;
static const Uuid invalid_uuid = UUID_INVALID_INIT;

void uuid_generate(Uuid *uuid_out) {
  uint8_t uuid_bytes[UUID_SIZE];
  uint32_t *uuid_words = (uint32_t*)uuid_bytes;
  for (size_t i = 0; i < UUID_SIZE / sizeof(uint32_t); i++) {
    uuid_words[i] = rand32();
  }
  // set the version bits
  uuid_bytes[6] = (uuid_bytes[6] & ~0xF0) | 0x40;
  // set the reserved bits
  uuid_bytes[8] = (uuid_bytes[8] & ~0xC0) | 0x80;

  // Use BE so that the bytes for version and reserved are correctly placed.
  *uuid_out = UuidMakeFromBEBytes(uuid_bytes);
}

bool uuid_equal(const Uuid *uu1, const Uuid *uu2) {
  if (uu1 == NULL || uu2 == NULL) {
    return false;
  }
  return memcmp(uu1, uu2, sizeof(Uuid)) == 0;
}

bool uuid_is_system(const Uuid *uuid) {
  return uuid_equal(uuid, &system_uuid);
}

bool uuid_is_invalid(const Uuid *uuid) {
  return !uuid || uuid_equal(uuid, &invalid_uuid);
}

void uuid_to_string(const Uuid *uuid, char *buffer) {
  if (!uuid) {
    strcpy(buffer, "{NULL UUID}");
    return;
  }

  // For reference...
  // {12345678-1234-5678-1234-567812345678}
  *buffer++ = '{';

  for (uint32_t i = 0; i < sizeof(Uuid); i++) {
    if ((i >= 4) && (i <= 10) && ((i % 2) == 0)) {
      *buffer++ = '-';
    }

    buffer += snprintf(buffer, 3, "%02"PRIx8, ((uint8_t *)uuid)[i]);
  }
  *buffer++ = '}';
  *buffer = '\0';
}
