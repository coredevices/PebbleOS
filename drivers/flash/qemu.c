/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/flash/qemu.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>

#include <board/board.h>

#define FLASH_CMD    0x00U
#define FLASH_ADDR   0x04U
#define FLASH_STATUS 0x08U
// The device persists the written range to its backing file when FLASH_SYNC is written with its
// start address, after FLASH_SYNC_LEN. Erases persist on their own.
#define FLASH_SYNC_LEN 0x18U
#define FLASH_SYNC     0x1CU

#define CMD_ERASE_SUBSECTOR 1U
#define CMD_ERASE_SECTOR    2U

#define STATUS_BUSY PBL_BIT(0)

static const struct pbl_flash_geometry s_geometry = {
  .size = 32U * 1024U * 1024U,
  .page_size = 256U,
  .sector_size = 64U * 1024U,
  .subsector_size = 4U * 1024U,
  .sector_erase_ms = 1U,
  .subsector_erase_ms = 1U,
};

static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  memcpy(buf, (const void *)(uintptr_t)addr, len);

  return 0;
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  const uint32_t page_size = flash->geometry->page_size;
  const uint8_t *src = buf;

  while (len > 0U) {
    size_t chunk = MIN(len, page_size - (addr % page_size));
    volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)addr;

    for (size_t i = 0U; i < chunk; i++) {
      dst[i] = src[i];
    }

    pbl_writel(chunk, QEMU_EXTFLASH_BASE + FLASH_SYNC_LEN);
    pbl_writel(addr, QEMU_EXTFLASH_BASE + FLASH_SYNC);

    addr += chunk;
    src += chunk;
    len -= chunk;
  }

  return 0;
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  uint32_t cmd;

  if (size == flash->geometry->sector_size) {
    cmd = CMD_ERASE_SECTOR;
  } else if (size == flash->geometry->subsector_size) {
    cmd = CMD_ERASE_SUBSECTOR;
  } else {
    return -EINVAL;
  }

  pbl_writel(addr, QEMU_EXTFLASH_BASE + FLASH_ADDR);
  pbl_writel(cmd, QEMU_EXTFLASH_BASE + FLASH_CMD);

  return 0;
}

static int prv_erase_status(const struct pbl_flash *flash) {
  return ((pbl_readl(QEMU_EXTFLASH_BASE + FLASH_STATUS) & STATUS_BUSY) != 0U) ? -EBUSY : 0;
}

static const struct pbl_flash_ops s_ops = {
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
  .erase_status = prv_erase_status,
};

PBL_FLASH_STATE_DEFINE(pbl_flash_qemu);
const struct pbl_flash pbl_flash_qemu =
    PBL_FLASH_INIT(pbl_flash_qemu, "flash", nullptr, nullptr, &s_ops, QEMU_EXTFLASH_XIP_BASE,
                   &s_geometry, nullptr, nullptr);
PBL_DEVICE_REGISTER(pbl_flash_qemu, &pbl_flash_qemu.dev);
