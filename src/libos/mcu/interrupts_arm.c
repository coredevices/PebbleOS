/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mcu/interrupts.h"

bool mcu_state_are_interrupts_enabled(void) {
  // When this bit is set, all interrupts (of configureable priority) are disabled
  return ((__get_PRIMASK() & 0x1) == 0x0);
}
