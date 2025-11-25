/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/mcu.h"

#include <bf0_hal_efuse.h>

#define EFUSE_UID_OFFSET 0
#define EFUSE_UID_SIZE   16

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  if (*buf_sz < EFUSE_UID_SIZE) {
    return E_OUT_OF_MEMORY;
  }

  HAL_EFUSE_Read(EFUSE_UID_OFFSET, (uint8_t *)buf, EFUSE_UID_SIZE);
  *buf_sz = EFUSE_UID_SIZE;

  return S_SUCCESS;
}

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks) {
  return ((cpu_ticks * 1000) / SystemCoreClock);
}
