/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/** @cond INTERNAL_HIDDEN */

static inline bool mcu_state_is_isr(void) {
  uint32_t ipsr;

  __asm volatile("mrs %0, ipsr" : "=r"(ipsr));

  return ipsr != 0U;
}

/** @endcond */
