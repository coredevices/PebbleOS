/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/mcu.h>
#include <pbl/drivers/nvmem/stm32_romem.h>

#define UID_OFFSET 0U
#define UID_SIZE   12U

static const struct pbl_nvmem_cell s_uid_cell = PBL_NVMEM_CELL(STM32_ROMEM, UID_OFFSET, UID_SIZE);

int mcu_get_serial(void *buf, size_t *buf_sz) {
  int ret;

  if (*buf_sz < UID_SIZE) {
    return -ENOSPC;
  }

  ret = pbl_device_init(&s_uid_cell.nvmem->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_nvmem_cell_read(&s_uid_cell, buf, UID_SIZE);
  if (ret != 0) {
    return ret;
  }

  *buf_sz = UID_SIZE;

  return 0;
}
