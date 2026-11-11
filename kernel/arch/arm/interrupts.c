/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <pbl/mcu/interrupts.h>

bool mcu_state_are_interrupts_enabled(void) {
  // When this bit is set, all interrupts (of configurable priority) are disabled
  return (arm_primask_get() & ARM_PRIMASK_PM) == 0U;
}
