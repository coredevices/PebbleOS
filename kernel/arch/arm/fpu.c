/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_cpu.h"

#include <stdint.h>

#include <pbl/mcu/fpu.h>

void mcu_fpu_cleanup(void) {
  // The lazy stacking mechanism for the Cortex M4 starts stacking FPU
  // registers during context switches once the thread has used the FPU
  // once. This is problematic because this bumps the stack cost of a context
  // switch by an additional 132 bytes. This routine resets the FPCA bit which
  // controls whether or not this stacking takes place
  // For the Cortex M3, this routine is a no-op

  uint32_t control = arm_control_get();

  if ((control & ARM_CONTROL_FPCA) != 0) {
    arm_control_set(control & ~ARM_CONTROL_FPCA);
  }
}
