/* SPDX-FileCopyrightText: 2016 Intel Corporation */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SETJMP_H
#define SETJMP_H

#include <stdint.h>

typedef uint64_t jmp_buf[14];

extern  int setjmp (jmp_buf env);
extern  void longjmp (jmp_buf env, int val);

#endif /* !SETJMP_H */
