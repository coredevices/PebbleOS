/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/mcu.h>

#include <string.h>

static const uint8_t s_serial[] = {'Q', 'E', 'M', 'U', '0', '0', '0', '0'};

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  if (*buf_sz < sizeof(s_serial)) {
    return E_OUT_OF_MEMORY;
  }

  memcpy(buf, s_serial, sizeof(s_serial));
  *buf_sz = sizeof(s_serial);

  return S_SUCCESS;
}
