/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "delay.h"

#include <inttypes.h>

void delay_us(uint32_t us) {
  // Empirically (measured on a bb2), 1 loop = ~47ns. (sysclk @
  // 64MHz, prefetch disabled) Alignment of code will have some impact on how
  // long this actually takes
  uint32_t delay_loops = us * 22;

  __asm volatile (
      "spinloop:                             \n"
      "  subs %[delay_loops], #1             \n"
      "  bne spinloop                        \n"
      : [delay_loops] "+r" (delay_loops) // read-write operand
      :
      : "cc"
       );
}

void delay_ms(uint32_t millis) {
  // delay_us(millis*1000) is not used because a long delay could easily
  // overflow the veriable. Without the outer loop, a delay of even five
  // seconds would overflow.
  while (millis--) {
    delay_us(1000);
  }
}
