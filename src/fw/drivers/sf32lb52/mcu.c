/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "drivers/mcu.h"

#include <bf0_hal_efuse.h>

#define EFUSE_UID_OFFSET 0
#define EFUSE_UID_SIZE   16

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  if (*buf_sz < EFUSE_UID_SIZE) {
    return E_OUT_OF_MEMORY;
  }
  uint8_t serial[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};
  *buf_sz = EFUSE_UID_SIZE;
  for (int i = 0; i < EFUSE_UID_SIZE; i++) {
    ((uint8_t *)buf)[i] = serial[i];
  }

  // HAL_EFUSE_Read(EFUSE_UID_OFFSET, (uint8_t *)buf, EFUSE_UID_SIZE);
  // *buf_sz = EFUSE_UID_SIZE;

  return S_SUCCESS;
}

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks) {
  return ((cpu_ticks * 1000) / SystemCoreClock);
}
