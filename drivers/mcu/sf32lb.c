/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/mcu.h>
#include <pbl/drivers/nvmem/sf32lb.h>

#if defined(CONFIG_SOC_SF32LB57)
#define EFUSE SF32LB57_EFUSE
#else
#define EFUSE SF32LB52_EFUSE
#endif

#define EFUSE_UID_OFFSET 0U
#define EFUSE_UID_SIZE   16U

static const struct pbl_nvmem_cell s_uid_cell =
    PBL_NVMEM_CELL(EFUSE, EFUSE_UID_OFFSET, EFUSE_UID_SIZE);

int mcu_get_serial(void *buf, size_t *buf_sz) {
  const uint8_t *id = buf;
  int ret;

  if (*buf_sz < EFUSE_UID_SIZE) {
    return -ENOSPC;
  }

  ret = pbl_device_init(&s_uid_cell.nvmem->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_nvmem_cell_read(&s_uid_cell, buf, EFUSE_UID_SIZE);
  if (ret != 0) {
    return ret;
  }

  // Parts that have not been through factory test have a blank eFuse
  for (size_t i = 0U; i < EFUSE_UID_SIZE; i++) {
    if (id[i] != 0U) {
      *buf_sz = EFUSE_UID_SIZE;
      return 0;
    }
  }

  return -ENODATA;
}
