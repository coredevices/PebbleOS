/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define CMSIS_COMPATIBLE
#include <mcu.h>

static inline bool mcu_state_is_isr(void) {
  return __get_IPSR() != 0;
}

static inline uint32_t mcu_state_get_isr_priority(void) {
  uint32_t exc_number  = __get_IPSR();
  if (exc_number == 0) {
    return ~0;
  }
  // Exception numbers 0 -> 15 are "internal" interrupts and NVIC_GetPriority() expects them to be
  // negative numbers.
  return NVIC_GetPriority((int)exc_number - 16);
}
