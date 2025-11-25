/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

static inline bool mcu_state_is_isr(void) {
  return false;
}

static inline uint32_t mcu_state_get_isr_priority(void) {
  return ~0;
}
