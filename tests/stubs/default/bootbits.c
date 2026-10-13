/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/compiler.h>

#include <system/bootbits.h>

PBL_WEAK void boot_bit_clear(BootBitValue bit) {
  return;
}

PBL_WEAK bool boot_bit_test(BootBitValue bit) {
  return (false);
}
