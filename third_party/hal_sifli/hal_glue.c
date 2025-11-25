/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>

extern int rtc_get_ticks(void);

uint32_t HAL_GetTick(void) {
  return (uint32_t)rtc_get_ticks();
}
