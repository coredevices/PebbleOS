/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/attributes.h"

#include "wfi.h"

void NOINLINE NAKED_FUNC do_wfi(void) {
#if MICRO_FAMILY_QEMU
  // QEMU Cortex-M33 doesn't properly wake from WFI when PRIMASK=1.
  // Use WFE which yields to QEMU's scheduler and wakes on events.
  __asm volatile (
      "wfe      \n"
      "bx lr    \n"
      );
#else
  // Work around a very strange bug in the STM32F where, upon waking from
  // STOP or SLEEP mode, the processor begins acting strangely depending on the
  // contents of the bytes following the "bx lr" instruction.
  __asm volatile (
      ".align 4 \n"  // Force 16-byte alignment
      "wfi      \n"  // This instruction cannot be placed at 0xnnnnnnn4
      "nop      \n"
      "bx lr    \n"
      "nop      \n"  // Fill the rest of the cache line with NOPs as the bytes
      "nop      \n"  // following the bx affect the processor for some reason.
      "nop      \n"
      "nop      \n"
      "nop      \n"
      );
#endif
}
