/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/flash/flash_impl.h"
#include "drivers/flash/qspi_flash.h"
#include "drivers/flash/qspi_flash_part_definitions.h"
#include "flash_region/flash_region.h"
#include "system/passert.h"
#include "system/status_codes.h"
#include "system/version.h"
#include "util/math.h"

#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

static QSPIFlashPart QSPI_FLASH_PART = {
    .instructions =
        {
            .fast_read = 0x0B,
            .pp = 0x02,
            .erase_sector_4k = 0x20,
            .erase_block_64k = 0xD8,
            .write_enable = 0x06,
            .write_disable = 0x04,
            .rdsr1 = 0x05,
            .rdsr2 = 0x2B,
            .erase_suspend = 0xB0,
            .erase_resume = 0x30,
            .enter_low_power = 0xB9,
            .exit_low_power = 0xAB,
            .enter_quad_mode = 0x35,
            .reset_enable = 0x66,
            .reset = 0x99,
            .qspi_id = 0xAF,

            .block_lock = 0x36,
            .block_lock_status = 0x3C,
            .block_unlock_all = 0x98,

            .write_protection_enable = 0x68,
            .read_protection_status = 0x2B,
        },
    .status_bit_masks =
        {
            .busy = 1 << 0,
            .write_enable = 1 << 1,
        },
    .flag_status_bit_masks =
        {
            .erase_suspend = 1 << 3,
        },
    .dummy_cycles =
        {
            .fast_read = 4,
        },
    .block_lock =
        {
            .has_lock_data = false,
            .locked_check = 0xff,

            .protection_enabled_mask = (1 << 7),
        },
    .reset_latency_ms = 13,
    .suspend_to_read_latency_us = 20,
    .standby_to_low_power_latency_us = 10,
    .low_power_to_standby_latency_us = 30,
    .supports_fast_read_ddr = false,
    .supports_block_lock = true,
    .qspi_id_value = 0x3725c2,
    .name = "MX25U64",
};

//! Any PRF built after this timestamp supports mx25u flash protection
#define MIN_PRF_TIMESTAMP_SUPPORTING_PROTECTION (1466531458)

//! True if the installed PRF version supports flash protection
static bool s_flash_protection_supported = false;

bool flash_check_whoami(void) { return qspi_flash_check_whoami(QSPI_FLASH); }

FlashAddress flash_impl_get_sector_base_address(FlashAddress addr) {
  return (addr & SECTOR_ADDR_MASK);
}

FlashAddress flash_impl_get_subsector_base_address(FlashAddress addr) {
  return (addr & SUBSECTOR_ADDR_MASK);
}

static bool prv_prf_supports_flash_protection(void) {
#if IS_BIGBOARD
  // Bigboards should always exercise flash protection
  return true;
#else
  FirmwareMetadata prf;
  if (!version_copy_recovery_fw_metadata(&prf)) {
    return false;
  }
  return (prf.version_timestamp > MIN_PRF_TIMESTAMP_SUPPORTING_PROTECTION);
#endif
}

void flash_impl_enable_write_protection(void) {
  s_flash_protection_supported = prv_prf_supports_flash_protection();

  if (s_flash_protection_supported) {
    // Ensure that write protection is enabled on the mx25u
    if (qspi_flash_write_protection_enable(QSPI_FLASH) == S_SUCCESS) {
      // after flash protection is enabled, full array is locked. Unlock it.
      qspi_flash_unlock_all(QSPI_FLASH);
    }
  }
}

status_t flash_impl_write_protect(FlashAddress start_sector, FlashAddress end_sector) {
  if (!s_flash_protection_supported) {
    return S_SUCCESS;  // If not supported, pretend protection succeeded.
  }

  FlashAddress block_addr = start_sector;
  while (block_addr <= end_sector) {
    uint32_t block_size;
    if (WITHIN(block_addr, SECTOR_SIZE_BYTES, BOARD_NOR_FLASH_SIZE - SECTOR_SIZE_BYTES - 1)) {
      // Middle of flash has 64k lock units
      block_addr = flash_impl_get_sector_base_address(block_addr);
      block_size = SECTOR_SIZE_BYTES;
    } else {
      // Start and end of flash have 1 sector of 4k lock units
      block_addr = flash_impl_get_subsector_base_address(block_addr);
      block_size = SUBSECTOR_SIZE_BYTES;
    }
    const status_t sc = qspi_flash_lock_sector(QSPI_FLASH, block_addr);
    if (FAILED(sc)) {
      return sc;
    }
    block_addr += block_size;
  }

  return S_SUCCESS;
}

status_t flash_impl_unprotect(void) { return qspi_flash_unlock_all(QSPI_FLASH); }

status_t flash_impl_init(bool coredump_mode) {
  qspi_flash_init(QSPI_FLASH, &QSPI_FLASH_PART, coredump_mode);
  qspi_flash_unlock_all(QSPI_FLASH);
  return S_SUCCESS;
}

status_t flash_impl_get_erase_status(void) { return qspi_flash_is_erase_complete(QSPI_FLASH); }

status_t flash_impl_erase_subsector_begin(FlashAddress subsector_addr) {
  return qspi_flash_erase_begin(QSPI_FLASH, subsector_addr, true /* is_subsector */);
}
status_t flash_impl_erase_sector_begin(FlashAddress sector_addr) {
  return qspi_flash_erase_begin(QSPI_FLASH, sector_addr, false /* !is_subsector */);
}

status_t flash_impl_erase_suspend(FlashAddress sector_addr) {
  return qspi_flash_erase_suspend(QSPI_FLASH, sector_addr);
}

status_t flash_impl_erase_resume(FlashAddress sector_addr) {
  qspi_flash_erase_resume(QSPI_FLASH, sector_addr);
  return S_SUCCESS;
}

status_t flash_impl_read_sync(void *buffer_ptr, FlashAddress start_addr, size_t buffer_size) {
  PBL_ASSERT(buffer_size > 0, "flash_impl_read_sync() called with 0 bytes to read");
  qspi_flash_read_blocking(QSPI_FLASH, start_addr, buffer_ptr, buffer_size);
  return S_SUCCESS;
}

int flash_impl_write_page_begin(const void *buffer, const FlashAddress start_addr, size_t len) {
  return qspi_flash_write_page_begin(QSPI_FLASH, buffer, start_addr, len);
}

status_t flash_impl_get_write_status(void) { return qspi_flash_get_write_status(QSPI_FLASH); }

status_t flash_impl_enter_low_power_mode(void) {
  qspi_flash_set_lower_power_mode(QSPI_FLASH, true);
  return S_SUCCESS;
}
status_t flash_impl_exit_low_power_mode(void) {
  qspi_flash_set_lower_power_mode(QSPI_FLASH, false);
  return S_SUCCESS;
}

status_t flash_impl_set_burst_mode(bool burst_mode) {
  // NYI
  return S_SUCCESS;
}

status_t flash_impl_blank_check_sector(FlashAddress addr) {
  return qspi_flash_blank_check(QSPI_FLASH, addr, false /* !is_subsector */);
}
status_t flash_impl_blank_check_subsector(FlashAddress addr) {
  return qspi_flash_blank_check(QSPI_FLASH, addr, true /* is_subsector */);
}

uint32_t flash_impl_get_typical_sector_erase_duration_ms(void) { return 400; }

uint32_t flash_impl_get_typical_subsector_erase_duration_ms(void) { return 40; }
