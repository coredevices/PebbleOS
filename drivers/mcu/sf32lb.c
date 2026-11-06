/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/mcu.h>
#include <pbl/drivers/nvmem/sf32lb52.h>

#define EFUSE_UID_OFFSET 0U
#define EFUSE_UID_SIZE   16U

static const struct pbl_nvmem_cell s_uid_cell =
    PBL_NVMEM_CELL(SF32LB52_EFUSE, EFUSE_UID_OFFSET, EFUSE_UID_SIZE);

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  if (*buf_sz < EFUSE_UID_SIZE) {
    return E_OUT_OF_MEMORY;
  }

  if (pbl_nvmem_cell_read(&s_uid_cell, buf, EFUSE_UID_SIZE) != 0) {
    return E_ERROR;
  }

  *buf_sz = EFUSE_UID_SIZE;

  return S_SUCCESS;
}
