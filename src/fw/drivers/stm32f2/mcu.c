/*
 * Copyright 2024 Google LLC
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
#include <string.h>

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#if MICRO_FAMILY_STM32F7
static const uint8_t STM32_UNIQUE_DEVICE_ID_ADDR[] = {0x1f, 0xf0, 0xf4, 0x20};
#else
static const uint8_t STM32_UNIQUE_DEVICE_ID_ADDR[] = {0x1f, 0xff, 0x7a, 0x10};
#endif

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  if (*buf_sz < sizeof(STM32_UNIQUE_DEVICE_ID_ADDR)) {
    return E_OUT_OF_MEMORY;
  }

  memcpy(buf, STM32_UNIQUE_DEVICE_ID_ADDR, sizeof(STM32_UNIQUE_DEVICE_ID_ADDR));
  *buf_sz = sizeof(STM32_UNIQUE_DEVICE_ID_ADDR);

  return S_SUCCESS;
}

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks) {
  RCC_ClocksTypeDef clocks;
  RCC_GetClocksFreq(&clocks);
  return ((cpu_ticks * 1000) / clocks.HCLK_Frequency);
}
