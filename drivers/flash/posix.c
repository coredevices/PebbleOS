/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/flash/flash_impl.h>

#include <pbl/logging/logging.h>

#include "flash_region/flash_region.h"
#include "posix_host.h"
#include "system/passert.h"
#include "system/status_codes.h"

// External flash, as a file mapped into memory. Writes and erases land in
// the file right away, so its contents survive a restart.

#define PRV_CAPACITY       0x2000000
#define PRV_SECTOR_SIZE    0x10000
#define PRV_SUBSECTOR_SIZE 0x1000
#define PRV_PAGE_SIZE      256

static uint8_t *s_flash;
static const char *s_flash_path = "pebbleos-flash.bin";
static const char *s_resources_path;

static void prv_set_flash_path(const char *value) {
  s_flash_path = value;
}

static void prv_set_resources_path(const char *value) {
  s_resources_path = value;
}

POSIX_HOST_OPTION(.flag = 'f', .arg = "flash.bin",
                  .help = "file backing the external flash, created if missing",
                  .set = prv_set_flash_path)
POSIX_HOST_OPTION(.flag = 'r', .arg = "res.pbpack",
                  .help = "system resources to install at boot (default: the build's)",
                  .set = prv_set_resources_path)

static uint8_t *prv_ptr(FlashAddress addr) {
  return &s_flash[addr & (PRV_CAPACITY - 1)];
}

static void prv_install_resources(void) {
  // The build's resources sit next to the executable.
  char default_path[PATH_MAX];
  const char *path = s_resources_path;
  if (path == NULL) {
    snprintf(default_path, sizeof(default_path), "%s/system_resources.pbpack",
             posix_host_exe_dir());
    path = default_path;
  }
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    PBL_LOG_WRN("cannot open resources %s", path);
    return;
  }
  const size_t bank_size =
      FLASH_REGION_SYSTEM_RESOURCES_BANK_0_END - FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN;
  uint8_t *bank = prv_ptr(FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN);
  size_t len = fread(bank, 1, bank_size, f);
  fclose(f);
  memset(bank + len, 0xff, bank_size - len);
}

status_t flash_impl_init(bool coredump_mode) {
  if (s_flash != NULL) {
    return S_SUCCESS;
  }
  int fd = open(s_flash_path, O_RDWR | O_CREAT, 0644);
  PBL_ASSERT(fd >= 0, "cannot open flash file");

  struct stat st;
  fstat(fd, &st);
  bool blank = st.st_size == 0;
  PBL_ASSERT(ftruncate(fd, PRV_CAPACITY) == 0, "cannot size flash file");

  s_flash = mmap(NULL, PRV_CAPACITY, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  PBL_ASSERT(s_flash != MAP_FAILED, "cannot map flash file");
  if (blank) {
    memset(s_flash, 0xff, PRV_CAPACITY);
  }

  prv_install_resources();
  return S_SUCCESS;
}

status_t flash_impl_set_burst_mode(bool enable) {
  return S_SUCCESS;
}

FlashAddress flash_impl_get_sector_base_address(FlashAddress addr) {
  return addr & ~(PRV_SECTOR_SIZE - 1);
}

FlashAddress flash_impl_get_subsector_base_address(FlashAddress addr) {
  return addr & ~(PRV_SUBSECTOR_SIZE - 1);
}

size_t flash_impl_get_capacity(void) {
  return PRV_CAPACITY;
}

status_t flash_impl_enter_low_power_mode(void) {
  return S_SUCCESS;
}

status_t flash_impl_exit_low_power_mode(void) {
  return S_SUCCESS;
}

status_t flash_impl_read_sync(void *buffer, FlashAddress addr, size_t len) {
  memcpy(buffer, prv_ptr(addr), len);
  return S_SUCCESS;
}

void flash_impl_enable_write_protection(void) {
}

status_t flash_impl_write_protect(FlashAddress start_sector, FlashAddress end_sector) {
  return S_SUCCESS;
}

status_t flash_impl_unprotect(void) {
  return S_SUCCESS;
}

int flash_impl_write_page_begin(const void *buffer, FlashAddress addr, size_t len) {
  size_t page_remaining = PRV_PAGE_SIZE - (addr & (PRV_PAGE_SIZE - 1));
  size_t write_len = len < page_remaining ? len : page_remaining;
  uint8_t *dst = prv_ptr(addr);
  const uint8_t *src = buffer;
  // NOR flash only clears bits.
  for (size_t i = 0; i < write_len; i++) {
    dst[i] &= src[i];
  }
  return (int)write_len;
}

status_t flash_impl_get_write_status(void) {
  return S_SUCCESS;
}

status_t flash_impl_write_suspend(FlashAddress addr) {
  return S_SUCCESS;
}

status_t flash_impl_write_resume(FlashAddress addr) {
  return S_SUCCESS;
}

status_t flash_impl_erase_subsector_begin(FlashAddress subsector_addr) {
  memset(prv_ptr(flash_impl_get_subsector_base_address(subsector_addr)), 0xff, PRV_SUBSECTOR_SIZE);
  return S_SUCCESS;
}

status_t flash_impl_erase_sector_begin(FlashAddress sector_addr) {
  memset(prv_ptr(flash_impl_get_sector_base_address(sector_addr)), 0xff, PRV_SECTOR_SIZE);
  return S_SUCCESS;
}

status_t flash_impl_erase_bulk_begin(void) {
  memset(s_flash, 0xff, PRV_CAPACITY);
  return S_SUCCESS;
}

status_t flash_impl_get_erase_status(void) {
  return S_SUCCESS;
}

uint32_t flash_impl_get_typical_subsector_erase_duration_ms(void) {
  return 1;
}

uint32_t flash_impl_get_typical_sector_erase_duration_ms(void) {
  return 1;
}

status_t flash_impl_erase_suspend(FlashAddress addr) {
  return S_NO_ACTION_REQUIRED;
}

status_t flash_impl_erase_resume(FlashAddress addr) {
  return S_SUCCESS;
}

static status_t prv_blank_check(FlashAddress base, size_t size) {
  const uint8_t *p = prv_ptr(base);
  for (size_t i = 0; i < size; i++) {
    if (p[i] != 0xff) {
      return S_FALSE;
    }
  }
  return S_TRUE;
}

status_t flash_impl_blank_check_subsector(FlashAddress addr) {
  return prv_blank_check(flash_impl_get_subsector_base_address(addr), PRV_SUBSECTOR_SIZE);
}

status_t flash_impl_blank_check_sector(FlashAddress addr) {
  return prv_blank_check(flash_impl_get_sector_base_address(addr), PRV_SECTOR_SIZE);
}

void flash_impl_use(void) {
}

void flash_impl_release(void) {
}

void flash_impl_release_many(uint32_t num_locks) {
}

status_t flash_impl_read_security_register(uint32_t addr, uint8_t *val) {
  *val = 0xff;
  return S_SUCCESS;
}

status_t flash_impl_security_register_is_locked(uint32_t address, bool *locked) {
  *locked = false;
  return S_SUCCESS;
}

status_t flash_impl_erase_security_register(uint32_t addr) {
  return S_SUCCESS;
}

status_t flash_impl_write_security_register(uint32_t addr, uint8_t val) {
  return S_SUCCESS;
}

static const FlashSecurityRegisters s_security_regs = {0};

const FlashSecurityRegisters *flash_impl_security_registers_info(void) {
  return &s_security_regs;
}
