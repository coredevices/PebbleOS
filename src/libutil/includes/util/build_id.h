/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "util/attributes.h"

//! The linker inserts the build id as an "elf external note" structure:
typedef struct PACKED {
  uint32_t name_length;
  uint32_t data_length;
  uint32_t type; // NT_GNU_BUILD_ID = 3
  uint8_t data[]; // concatenated name ('GNU') + data (build id)
} ElfExternalNote;

// the build id is a unique identification for the built files. The default algo uses SHA1
// to produce a 160 bit (20 byte sequence)
#define BUILD_ID_EXPECTED_LEN    (20)

#define BUILD_ID_NAME_EXPECTED_LEN  (4)

#define BUILD_ID_TOTAL_EXPECTED_LEN (sizeof(ElfExternalNote) + \
                                     BUILD_ID_NAME_EXPECTED_LEN + BUILD_ID_EXPECTED_LEN)

bool build_id_contains_gnu_build_id(const ElfExternalNote *note);
