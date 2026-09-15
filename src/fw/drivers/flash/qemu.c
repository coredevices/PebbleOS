/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>

#include <errno.h>
#include <string.h>

#include "board/board.h"
#include "pbl/util/misc.h"

#define REG32(base, off) (*(volatile uint32_t *)((base) + (off)))

#define FLASH_CMD 0x00
#define FLASH_ADDR 0x04
#define FLASH_STATUS 0x08
// Persistence handshake: after each page write, QEMU is told the written range
// so that only complete pages (never torn writes) reach the backing file.
// Erases are persisted by QEMU on their own.
#define FLASH_SYNC_LEN 0x18
#define FLASH_SYNC 0x1C

#define CMD_ERASE_SUBSECTOR 1
#define CMD_ERASE_SECTOR 2

#define STATUS_BUSY (1 << 0)

#define PAGE_SIZE 256U
#define SUBSECTOR_SIZE 4096U
#define SECTOR_SIZE 65536U

struct pbl_flash_qemu {
  struct pbl_flash_device dev;
  //! Controller registers of the QEMU pebble-extflash device.
  uintptr_t regs;
};

static inline uintptr_t prv_regs(const struct pbl_flash_device *dev) {
  return container_of(dev, const struct pbl_flash_qemu, dev)->regs;
}

static int prv_init(const struct pbl_flash_device *dev) {
  return 0;
}

// Flash addresses are XIP addresses, so data can be accessed directly.
static int prv_read(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len) {
  memcpy(buf, (const void *)(uintptr_t)addr, len);
  return 0;
}

static int prv_write(const struct pbl_flash_device *dev, uint32_t addr, const void *buf,
                     size_t len) {
  uintptr_t regs = prv_regs(dev);
  const uint8_t *src = buf;

  while (len > 0) {
    size_t chunk = PAGE_SIZE - (addr % PAGE_SIZE);
    if (chunk > len) {
      chunk = len;
    }

    volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)addr;
    for (size_t i = 0; i < chunk; i++) {
      dst[i] = src[i];
    }
    REG32(regs, FLASH_SYNC_LEN) = chunk;
    REG32(regs, FLASH_SYNC) = addr;

    addr += chunk;
    src += chunk;
    len -= chunk;
  }

  return 0;
}

static int prv_erase_begin(const struct pbl_flash_device *dev, uint32_t addr, size_t size) {
  uintptr_t regs = prv_regs(dev);
  uint32_t cmd;

  if (size == SECTOR_SIZE) {
    cmd = CMD_ERASE_SECTOR;
  } else if (size == SUBSECTOR_SIZE) {
    cmd = CMD_ERASE_SUBSECTOR;
  } else {
    return -EINVAL;
  }

  REG32(regs, FLASH_ADDR) = addr;
  REG32(regs, FLASH_CMD) = cmd;
  while (REG32(regs, FLASH_STATUS) & STATUS_BUSY) {
  }

  return 0;
}

static const struct pbl_flash_ops s_ops = {
    .init = prv_init,
    .read = prv_read,
    .write = prv_write,
    .erase_begin = prv_erase_begin,
};

static const struct pbl_flash_geometry s_geometry = {
    .size = CONFIG_FLASH_QEMU_SIZE,
    .page_size = PAGE_SIZE,
    .sector_size = SECTOR_SIZE,
    .subsector_size = SUBSECTOR_SIZE,
    .sector_erase_ms = 1,
    .subsector_erase_ms = 1,
};
static struct pbl_flash_device_state s_flash_state;
static const struct pbl_flash_qemu s_flash = {
    .dev =
        {
            .state = &s_flash_state,
            .ops = &s_ops,
            .base = QEMU_EXTFLASH_XIP_BASE,
            .geometry = &s_geometry,
        },
    .regs = QEMU_EXTFLASH_BASE,
};
const struct pbl_flash_device *const FLASH = &s_flash.dev;
