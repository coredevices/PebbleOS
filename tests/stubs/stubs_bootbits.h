/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/bootbits.h"
#include <inttypes.h>
#include <stdbool.h>

void boot_bit_clear(BootBitValue bit) {
  return;
}

bool boot_bit_test(BootBitValue bit) {
  return (false);
}
