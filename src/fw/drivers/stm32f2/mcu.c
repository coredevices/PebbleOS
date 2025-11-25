/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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
