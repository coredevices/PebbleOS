/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/flash/qemu.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

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

static const struct pbl_flash_qemu *prv_qemu(const struct pbl_flash *flash) {
  return container_of(flash, const struct pbl_flash_qemu, flash);
}

static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  memcpy(buf, (const void *)(prv_qemu(flash)->mem + addr), len);

  return 0;
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  const struct pbl_flash_qemu *qemu = prv_qemu(flash);
  const uint32_t page_size = flash->geometry->page_size;
  const uint8_t *src = buf;

  addr += qemu->mem;

  while (len > 0U) {
    size_t chunk = MIN(len, page_size - (addr % page_size));
    volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)addr;

    for (size_t i = 0U; i < chunk; i++) {
      dst[i] = src[i];
    }

    pbl_writel(chunk, qemu->base + FLASH_SYNC_LEN);
    pbl_writel(addr, qemu->base + FLASH_SYNC);

    addr += chunk;
    src += chunk;
    len -= chunk;
  }

  return 0;
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  const struct pbl_flash_qemu *qemu = prv_qemu(flash);
  uint32_t cmd;

  if (size == flash->geometry->sector_size) {
    cmd = CMD_ERASE_SECTOR;
  } else if (size == flash->geometry->subsector_size) {
    cmd = CMD_ERASE_SUBSECTOR;
  } else {
    return -EINVAL;
  }

  pbl_writel(qemu->mem + addr, qemu->base + FLASH_ADDR);
  pbl_writel(cmd, qemu->base + FLASH_CMD);

  return 0;
}

static int prv_erase_status(const struct pbl_flash *flash) {
  return ((pbl_readl(prv_qemu(flash)->base + FLASH_STATUS) & STATUS_BUSY) != 0U) ? -EBUSY : 0;
}

const struct pbl_flash_ops pbl_flash_qemu_ops = {
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
  .erase_status = prv_erase_status,
};

#include PBL_DT_INSTANCES(qemu_pebble_extflash)
