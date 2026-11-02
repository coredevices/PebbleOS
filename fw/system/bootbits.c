/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdint.h>

#include <pbl/boot/version.h>
#include <pbl/drivers/retained.h>
#include <pbl/logging/logging.h>

#include <system/bootbits.h>

#define REG CONFIG_BOOT_BOOTBITS_RETAINED_REG

void boot_bit_init(void) {
  if (!boot_bit_test(BOOT_BIT_INITIALIZED)) {
    pbl_retained_write(REG, BOOT_BIT_INITIALIZED);
  }
}

void boot_bit_set(BootBitValue bit) {
  pbl_retained_write(REG, pbl_retained_read(REG) | bit);
}

void boot_bit_clear(BootBitValue bit) {
  pbl_retained_write(REG, pbl_retained_read(REG) & ~bit);
}

bool boot_bit_test(BootBitValue bit) {
  return (pbl_retained_read(REG) & bit) != 0U;
}

void boot_bit_dump(void) {
  PBL_LOG_DBG("0x%" PRIx32, pbl_retained_read(REG));
}

uint32_t boot_bits_get(void) {
  return pbl_retained_read(REG);
}

uint32_t boot_version_read(void) {
  return pbl_boot_version_get();
}
