/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mcu/fpu.h"

#define CMSIS_COMPATIBLE
#include <mcu.h>

#include <stdint.h>

void mcu_fpu_cleanup(void) {
  // The lazy stacking mechanism for the Cortex M4 starts stacking FPU
  // registers during context switches once the thread has used the FPU
  // once. This is problematic because this bumps the stack cost of a context
  // switch by an additional 132 bytes. This routine resets the FPCA bit which
  // controls whether or not this stacking takes place
  // For the Cortex M3, this routine is a no-op

  const uint32_t fpca_bit_mask = 0x4;
  uint32_t control = __get_CONTROL();

  if ((control & fpca_bit_mask) != 0) {
    control &= ~fpca_bit_mask;
    __set_CONTROL(control);
  }
}
