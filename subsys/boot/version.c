/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/boot/version.h>

#ifdef CONFIG_PBLBOOT
#include <pbl/drivers/flash.h>
#include <pbl/kernel/compiler.h>

#include <flash_region/flash_region.h>

#define PB_VERSION_MAGIC 0x50425652UL

struct pb_version {
  uint32_t magic;
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t tweak;
} PBL_PACKED;

static_assert(sizeof(struct pb_version) == 8, "pb_version struct must be 8 bytes");

uint32_t pbl_boot_version_get(void) {
  struct pb_version version;

  flash_read_bytes((uint8_t *)&version, FLASH_REGION_BOOTLOADER_END - sizeof(version),
                   sizeof(version));

  if (version.magic != PB_VERSION_MAGIC) {
    return 0U;
  }

  return ((uint32_t)version.major << 24) | ((uint32_t)version.minor << 16) |
         ((uint32_t)version.patch << 8) | (uint32_t)version.tweak;
}
#else
#include <pbl/drivers/retained.h>

uint32_t pbl_boot_version_get(void) {
  return pbl_retained_read(CONFIG_BOOT_VERSION_RETAINED_REG);
}
#endif
