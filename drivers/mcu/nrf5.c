/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/mcu.h>

#include <hal/nrf_ficr.h>
#include <nrfx.h>

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  const uint32_t id[2] = {
    nrf_ficr_deviceid_get(NRF_FICR, 0),
    nrf_ficr_deviceid_get(NRF_FICR, 1),
  };

  if (*buf_sz < sizeof(id)) {
    return E_OUT_OF_MEMORY;
  }

  memcpy(buf, id, sizeof(id));
  *buf_sz = sizeof(id);

  return S_SUCCESS;
}
