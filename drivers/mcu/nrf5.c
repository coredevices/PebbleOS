/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/mcu.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_ficr.h>

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  const uint32_t id[2] = {
    pbl_readl(NRF52_FICR_BASE + NRF52_FICR_DEVICEID_OFFSET(0)),
    pbl_readl(NRF52_FICR_BASE + NRF52_FICR_DEVICEID_OFFSET(1)),
  };

  if (*buf_sz < sizeof(id)) {
    return E_OUT_OF_MEMORY;
  }

  memcpy(buf, id, sizeof(id));
  *buf_sz = sizeof(id);

  return S_SUCCESS;
}
