/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Slot zero preserves the existing language-pack resource IDs and filename.
#define LANGUAGE_PACK_COUNT           4
#define LANGUAGE_PACK_RESOURCE_BASE   0x2000
#define LANGUAGE_PACK_RESOURCE_STRIDE 32
#define LANGUAGE_PACK_ADD_FILENAME    "/lang-add"

static inline uint32_t language_pack_resource_id(uint8_t slot, uint32_t resource_id,
                                                 uint32_t strings_resource_id) {
  return slot == 0 ? resource_id
                   : LANGUAGE_PACK_RESOURCE_BASE + slot * LANGUAGE_PACK_RESOURCE_STRIDE +
                         resource_id - strings_resource_id + 1;
}

static inline const char *language_pack_filename(uint8_t slot) {
  static const char *const names[LANGUAGE_PACK_COUNT] = {"/lang", "/lang1", "/lang2", "/lang3"};
  return slot < LANGUAGE_PACK_COUNT ? names[slot] : NULL;
}

// A slot is occupied by any file, even one with a bad header; only pack removal frees it.
bool language_pack_is_installed(uint8_t slot);
