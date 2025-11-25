/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define __need_size_t
#define __need_NULL
#include <stddef.h>

#define __need___va_list
#include <stdarg.h>

typedef struct {
} FILE;

// stdio.h isn't supposed to define va_list, so _need___va_list gives us __gnuc_va_list
// Let's define that to something less compiler-specific
#define  __VA_LIST __gnuc_va_list

int printf(const char *__restrict format, ...)
        __attribute__((format (printf, 1, 2)));
int sprintf(char * restrict str, const char * restrict format, ...)
        __attribute__((__format__(__printf__, 2, 3)));
int snprintf(char * restrict str, size_t size, const char * restrict format, ...)
        __attribute__((__format__(__printf__, 3, 4)));
int vsprintf(char * restrict str, const char * restrict format, __VA_LIST ap)
        __attribute__((__format__(__printf__, 2, 0)));
int vsnprintf(char * restrict str, size_t size, const char * restrict format, __VA_LIST ap)
        __attribute__((__format__(__printf__, 3, 0)));

#if !UNITTEST
#define sniprintf snprintf
#define vsniprintf vsnprintf
#endif
