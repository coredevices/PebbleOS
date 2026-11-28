/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl_arch_irq.h>

static inline bool mcu_state_is_isr(void) {
  return arch_riscv_isr_nesting != 0U;
}
