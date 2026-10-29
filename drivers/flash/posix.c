/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <pbl/drivers/flash/posix.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>

#include <fcntl.h>
#include <flash_region/flash_region.h>
#include <posix_host.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

PBL_LOG_MODULE_DECLARE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

#define ERASED_BYTE 0xFFU

static const struct pbl_flash_geometry s_geometry = {
  .size = BOARD_NOR_FLASH_SIZE,
  .page_size = PAGE_SIZE_BYTES,
  .sector_size = SECTOR_SIZE_BYTES,
  .subsector_size = SUBSECTOR_SIZE_BYTES,
  .sector_erase_ms = 1U,
  .subsector_erase_ms = 1U,
};

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

static uint8_t *prv_ptr(const struct pbl_flash *flash, uint32_t addr) {
  return &s_flash[addr - flash->base];
}

static void prv_install_resources(const struct pbl_flash *flash) {
  const size_t bank_size =
      FLASH_REGION_SYSTEM_RESOURCES_BANK_0_END - FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN;
  char default_path[PATH_MAX];
  const char *path = s_resources_path;
  uint8_t *bank;
  size_t len;
  FILE *f;

  // The build's resources sit next to the executable
  if (path == nullptr) {
    snprintf(default_path, sizeof(default_path), "%s/system_resources.pbpack",
             posix_host_exe_dir());
    path = default_path;
  }

  f = fopen(path, "rb");
  if (f == nullptr) {
    PBL_LOG_WRN("cannot open resources %s", path);
    return;
  }

  bank = prv_ptr(flash, FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN);
  len = fread(bank, 1, bank_size, f);
  fclose(f);
  memset(bank + len, ERASED_BYTE, bank_size - len);
}

static int prv_init(const struct pbl_flash *flash) {
  const size_t size = flash->geometry->size;
  struct stat st;
  bool blank;
  int fd;

  if (s_flash != nullptr) {
    return 0;
  }

  fd = open(s_flash_path, O_RDWR | O_CREAT, 0644);
  if (fd < 0) {
    PBL_LOG_ERR("cannot open flash file %s", s_flash_path);
    return -EIO;
  }

  if (fstat(fd, &st) != 0 || ftruncate(fd, size) != 0) {
    close(fd);
    return -EIO;
  }
  blank = st.st_size == 0;

  s_flash = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (s_flash == MAP_FAILED) {
    s_flash = nullptr;
    return -EIO;
  }

  if (blank) {
    memset(s_flash, ERASED_BYTE, size);
  }

  prv_install_resources(flash);

  return 0;
}

static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  memcpy(buf, prv_ptr(flash, addr), len);

  return 0;
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  uint8_t *dst = prv_ptr(flash, addr);
  const uint8_t *src = buf;

  // NOR flash only clears bits
  for (size_t i = 0U; i < len; i++) {
    dst[i] &= src[i];
  }

  return 0;
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  memset(prv_ptr(flash, addr), ERASED_BYTE, size);

  return 0;
}

static const struct pbl_flash_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
};

PBL_FLASH_STATE_DEFINE(pbl_flash_posix);
const struct pbl_flash pbl_flash_posix =
    PBL_FLASH_INIT(pbl_flash_posix, "flash", nullptr, nullptr, &s_ops, FLASH_REGION_BASE_ADDRESS,
                   &s_geometry, nullptr, nullptr);
PBL_DEVICE_REGISTER(pbl_flash_posix, &pbl_flash_posix.dev);
