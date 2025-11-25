/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/build_id.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

bool build_id_contains_gnu_build_id(const ElfExternalNote *note) {
  const uint32_t NT_GNU_BUILD_ID = 3;
  const char *expected_name = "GNU";
  const uint32_t expected_name_length = strlen(expected_name) + 1;
  return note->type == NT_GNU_BUILD_ID &&
         note->name_length == expected_name_length &&
         note->data_length == BUILD_ID_EXPECTED_LEN &&
         (strncmp((const char *) note->data, expected_name, expected_name_length) == 0);
}
