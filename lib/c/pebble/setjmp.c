/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Implements:
//   int setjmp(jmp_buf env);
//   void longjmp(jmp_buf buf, int value);
///////////////////////////////////////
// Notes:
//   This is entirely non-portable. It will need to be rewritten if we stop using ARM.
//   Unfortunately, there is no portable way to define these.
//   This means unit tests can't use our setjmp/longjmp either.

#include <include/setjmp.h>

#if __ARM_ARCH_ISA_THUMB >= 2
// Valid for anything with THUMB2

int __attribute__((__naked__)) setjmp(jmp_buf env) {
  __asm volatile (
  // move SP to a register we can store from and don't need to save
  "mov  %ip, %sp\n"
  // store all the registers
  "stmia  %r0!, {%r4-%r9, %sl, %fp, %ip, %lr}\n"
// using real FPU
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
  // store FP registers
  "vstmia %r0!, {%s16-%s31}\n"
  // store FPSCR
  "vmrs %r1, fpscr\n"
  "str  %r1, [%r0], #4\n"
#endif
  // return 0
  "mov  %r0, #0\n"
  "bx lr\n"
  );
}

void __attribute__((__naked__)) longjmp(jmp_buf buf, int value) {
  __asm volatile (
  // load all the registers
  "ldmia  %r0!, {%r4-%r9, %sl, %fp, %ip, %lr}\n"
  // load SP from a register we could load to and don't need to restore
  "mov  %sp, %ip\n"
// using real FPU
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
  // load FP registers
  "vldmia %r0!, {%s16-%s31}\n"
  // load FPSCR
  "ldr  %r2, [%r0], #4\n"
  "vmsr fpscr, %r2\n"
#endif
  // return value
  "movs %r0, %r1\n"
  // unless it's 0, in which case, return 1.
  "it eq\n"
  "moveq  %r0, #1\n"
  "bx lr\n"
  );
}
#else
// Undefined implementations, don't implement!
// That way we get a link-time error.
#endif
