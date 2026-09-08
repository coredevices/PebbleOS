/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t utf8_t;

uint32_t utf8_peek_codepoint(utf8_t *string, utf8_t **next_ptr) {
  return 0;
}

size_t utf8_copy_character(utf8_t *dest, utf8_t *origin, size_t length) {
  return 0;
}
