/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/mcu.h>
#include <pbl/drivers/nvmem/nrf_ficr.h>

#define DEVICEID_OFFSET 0x304U
#define DEVICEID_SIZE   8U

static const struct pbl_nvmem_cell s_deviceid_cell =
    PBL_NVMEM_CELL(NRF_FICR, DEVICEID_OFFSET, DEVICEID_SIZE);

int mcu_get_serial(void *buf, size_t *buf_sz) {
  int ret;

  if (*buf_sz < DEVICEID_SIZE) {
    return -ENOSPC;
  }

  ret = pbl_device_init(&s_deviceid_cell.nvmem->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_nvmem_cell_read(&s_deviceid_cell, buf, DEVICEID_SIZE);
  if (ret != 0) {
    return ret;
  }

  *buf_sz = DEVICEID_SIZE;

  return 0;
}
