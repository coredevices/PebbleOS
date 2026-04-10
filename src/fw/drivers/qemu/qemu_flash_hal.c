/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/flash.h"
#include "drivers/flash/flash_impl.h"

#include "board/board.h"
#include "system/passert.h"
#include "system/status_codes.h"

#include <string.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// External flash controller MMIO register offsets
#define FLASH_CMD         0x00
#define FLASH_ADDR        0x04
#define FLASH_STATUS      0x08
#define FLASH_INT_CTRL    0x0C
#define FLASH_INT_STATUS  0x10
#define FLASH_SIZE        0x14

// CMD values
#define CMD_ERASE_SUBSECTOR  1
#define CMD_ERASE_SECTOR     2
#define CMD_WRITE_ENABLE     3

// STATUS bits
#define STATUS_BUSY      (1 << 0)
#define STATUS_COMPLETE  (1 << 1)

// Standard flash geometry
#define QEMU_SECTOR_SIZE     0x10000   // 64 KB
#define QEMU_SUBSECTOR_SIZE  0x1000    // 4 KB
#define QEMU_PAGE_SIZE       256

static bool s_initialized;

status_t flash_impl_init(bool coredump_mode) {
  (void)coredump_mode;
  s_initialized = true;
  return S_SUCCESS;
}

status_t flash_impl_set_burst_mode(bool enable) {
  (void)enable;
  return S_SUCCESS;
}

FlashAddress flash_impl_get_sector_base_address(FlashAddress addr) {
  return addr & ~(QEMU_SECTOR_SIZE - 1);
}

FlashAddress flash_impl_get_subsector_base_address(FlashAddress addr) {
  return addr & ~(QEMU_SUBSECTOR_SIZE - 1);
}

size_t flash_impl_get_capacity(void) {
  return REG32(QEMU_EXTFLASH_BASE + FLASH_SIZE);
}

status_t flash_impl_enter_low_power_mode(void) {
  return S_SUCCESS;
}

status_t flash_impl_exit_low_power_mode(void) {
  return S_SUCCESS;
}

status_t flash_impl_read_sync(void *buffer, FlashAddress addr, size_t len) {
  // Flash addresses already include the XIP base (0x10000000), read directly
  const uint8_t *src = (const uint8_t *)(uintptr_t)addr;
  memcpy(buffer, src, len);
  return S_SUCCESS;
}

status_t flash_impl_read_dma_begin(void *buffer, FlashAddress addr, size_t len) {
  // No DMA in QEMU; do a synchronous read and report completion
  status_t result = flash_impl_read_sync(buffer, addr, len);
  flash_impl_on_read_dma_complete_from_isr(result);
  return result;
}

void flash_impl_enable_write_protection(void) {
  // No-op for QEMU
}

status_t flash_impl_write_protect(FlashAddress start_sector, FlashAddress end_sector) {
  (void)start_sector;
  (void)end_sector;
  return S_SUCCESS;
}

status_t flash_impl_unprotect(void) {
  return S_SUCCESS;
}

static size_t s_last_write_len;

int flash_impl_write_page_begin(const void *buffer, FlashAddress addr, size_t len) {
  // Calculate how many bytes we can write in this page
  size_t page_offset = addr & (QEMU_PAGE_SIZE - 1);
  size_t page_remaining = QEMU_PAGE_SIZE - page_offset;
  size_t write_len = (len < page_remaining) ? len : page_remaining;

  // Write directly to XIP region - addr already includes base
  volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)addr;
  const uint8_t *src = (const uint8_t *)buffer;
  for (size_t i = 0; i < write_len; i++) {
    dst[i] = src[i];
  }

  s_last_write_len = write_len;
  return (int)write_len;
}

status_t flash_impl_get_write_status(void) {
  // Writes complete immediately in QEMU
  return S_SUCCESS;
}

status_t flash_impl_write_suspend(FlashAddress addr) {
  (void)addr;
  return S_SUCCESS;
}

status_t flash_impl_write_resume(FlashAddress addr) {
  (void)addr;
  return S_SUCCESS;
}

status_t flash_impl_erase_subsector_begin(FlashAddress subsector_addr) {
  REG32(QEMU_EXTFLASH_BASE + FLASH_ADDR) = subsector_addr;
  REG32(QEMU_EXTFLASH_BASE + FLASH_CMD) = CMD_ERASE_SUBSECTOR;
  return S_SUCCESS;
}

status_t flash_impl_erase_sector_begin(FlashAddress sector_addr) {
  REG32(QEMU_EXTFLASH_BASE + FLASH_ADDR) = sector_addr;
  REG32(QEMU_EXTFLASH_BASE + FLASH_CMD) = CMD_ERASE_SECTOR;
  return S_SUCCESS;
}

status_t flash_impl_erase_bulk_begin(void) {
  // Erase the entire flash by erasing sector by sector
  size_t capacity = flash_impl_get_capacity();
  for (size_t addr = 0; addr < capacity; addr += QEMU_SECTOR_SIZE) {
    flash_impl_erase_sector_begin(addr);
    // Wait for completion
    while (flash_impl_get_erase_status() == E_BUSY) {
      // spin
    }
  }
  return S_SUCCESS;
}

status_t flash_impl_get_erase_status(void) {
  uint32_t status = REG32(QEMU_EXTFLASH_BASE + FLASH_STATUS);
  if (status & STATUS_BUSY) {
    return E_BUSY;
  }
  return S_SUCCESS;
}

uint32_t flash_impl_get_typical_subsector_erase_duration_ms(void) {
  return 1;  // QEMU erases are instantaneous
}

uint32_t flash_impl_get_typical_sector_erase_duration_ms(void) {
  return 1;  // QEMU erases are instantaneous
}

status_t flash_impl_erase_suspend(FlashAddress addr) {
  (void)addr;
  // Check if erase already completed
  if (flash_impl_get_erase_status() == S_SUCCESS) {
    return S_NO_ACTION_REQUIRED;
  }
  return S_SUCCESS;
}

status_t flash_impl_erase_resume(FlashAddress addr) {
  (void)addr;
  return S_SUCCESS;
}

status_t flash_impl_blank_check_subsector(FlashAddress addr) {
  FlashAddress base = flash_impl_get_subsector_base_address(addr);
  const uint8_t *p = (const uint8_t *)(uintptr_t)base;
  for (size_t i = 0; i < QEMU_SUBSECTOR_SIZE; i++) {
    if (p[i] != 0xFF) {
      return S_FALSE;
    }
  }
  return S_TRUE;
}

status_t flash_impl_blank_check_sector(FlashAddress addr) {
  FlashAddress base = flash_impl_get_sector_base_address(addr);
  const uint8_t *p = (const uint8_t *)(uintptr_t)base;
  for (size_t i = 0; i < QEMU_SECTOR_SIZE; i++) {
    if (p[i] != 0xFF) {
      return S_FALSE;
    }
  }
  return S_TRUE;
}

void flash_impl_use(void) {
  // No power management needed for QEMU flash
}

void flash_impl_release(void) {
}

void flash_impl_release_many(uint32_t num_locks) {
  (void)num_locks;
}

status_t flash_impl_read_security_register(uint32_t addr, uint8_t *val) {
  (void)addr;
  *val = 0xFF;
  return S_SUCCESS;
}

status_t flash_impl_security_register_is_locked(uint32_t address, bool *locked) {
  (void)address;
  *locked = false;
  return S_SUCCESS;
}

status_t flash_impl_erase_security_register(uint32_t addr) {
  (void)addr;
  return S_SUCCESS;
}

status_t flash_impl_write_security_register(uint32_t addr, uint8_t val) {
  (void)addr;
  (void)val;
  return S_SUCCESS;
}

static const FlashSecurityRegisters s_security_regs = {
  .sec_regs = NULL,
  .num_sec_regs = 0,
  .sec_reg_size = 0,
};

const FlashSecurityRegisters *flash_impl_security_registers_info(void) {
  return &s_security_regs;
}
