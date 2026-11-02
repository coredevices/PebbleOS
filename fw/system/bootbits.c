/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdint.h>

#include <pbl/drivers/retained.h>
#include <pbl/logging/logging.h>

#include <system/bootbits.h>

#ifdef CONFIG_PBLBOOT
#include <pbl/drivers/flash.h>
#include <pbl/kernel/compiler.h>

#include <flash_region/flash_region.h>
#endif

void boot_bit_init(void) {
  if (!boot_bit_test(BOOT_BIT_INITIALIZED)) {
    pbl_retained_write(PBL_RETAINED_BOOTBITS, BOOT_BIT_INITIALIZED);
  }
}

void boot_bit_set(BootBitValue bit) {
  pbl_retained_write(PBL_RETAINED_BOOTBITS, pbl_retained_read(PBL_RETAINED_BOOTBITS) | bit);
}

void boot_bit_clear(BootBitValue bit) {
  pbl_retained_write(PBL_RETAINED_BOOTBITS, pbl_retained_read(PBL_RETAINED_BOOTBITS) & ~bit);
}

bool boot_bit_test(BootBitValue bit) {
  return (pbl_retained_read(PBL_RETAINED_BOOTBITS) & bit) != 0U;
}

void boot_bit_dump(void) {
  PBL_LOG_DBG("0x%" PRIx32, pbl_retained_read(PBL_RETAINED_BOOTBITS));
}

uint32_t boot_bits_get(void) {
  return pbl_retained_read(PBL_RETAINED_BOOTBITS);
}

#ifdef CONFIG_PBLBOOT
#define PB_VERSION_MAGIC 0x50425652UL

struct pb_version {
  uint32_t magic;
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t tweak;
} PBL_PACKED;

static_assert(sizeof(struct pb_version) == 8, "pb_version struct must be 8 bytes");

uint32_t boot_version_read(void) {
  struct pb_version version_data;
  uint32_t version;

  flash_read_bytes((uint8_t *)&version_data,
                   FLASH_REGION_BOOTLOADER_END - sizeof(struct pb_version),
                   sizeof(struct pb_version));

  if (version_data.magic != PB_VERSION_MAGIC) {
    return 0UL;
  }

  version = ((uint32_t)version_data.major << 24) | ((uint32_t)version_data.minor << 16) |
            ((uint32_t)version_data.patch << 8) | (uint32_t)version_data.tweak;

  return version;
}
#else
uint32_t boot_version_read(void) {
  return pbl_retained_read(PBL_RETAINED_BOOT_VERSION);
}
#endif
