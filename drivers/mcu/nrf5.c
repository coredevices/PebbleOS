/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/mcu.h>
#include <pbl/kernel/mmio.h>

#include <devicetree/regs.h>
#include <hardware/nrf52_ficr.h>

int mcu_get_serial(void *buf, size_t *buf_sz) {
  const uint32_t id[2] = {
    pbl_readl(PBL_DT_REG_FICR + NRF52_FICR_DEVICEID_OFFSET(0)),
    pbl_readl(PBL_DT_REG_FICR + NRF52_FICR_DEVICEID_OFFSET(1)),
  };

  if (*buf_sz < sizeof(id)) {
    return -ENOSPC;
  }

  memcpy(buf, id, sizeof(id));
  *buf_sz = sizeof(id);

  return 0;
}
