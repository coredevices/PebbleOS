/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

///////////////////////////////////////
// Notes:
//   This is entirely non-portable. It will need to be rewritten if we stop using ARM.
//   Unfortunately, there is no portable way to define these.
//   This means unit tests can't use our setjmp/longjmp either.

#pragma once

#if __arm__
// On ARM at least, GPRs are longs
// This still holds on A64.
struct __jmp_buf_struct {
  long r4, r5, r6, r7, r8, r9, sl, fp, sp, lr;
// Using real FPU
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
  // FPU registers are still 32bit on A64 though, so they're ints
  int s[16]; /* s16~s31 */
  int fpscr;
#endif
};
typedef struct __jmp_buf_struct jmp_buf[1];

int setjmp(jmp_buf env);
void longjmp(jmp_buf buf, int value);
#else
// other implementations either use system setjmp or don't have it.
# include_next <setjmp.h>
#endif
