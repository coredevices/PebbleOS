/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "logging.h"


void passert_failed(const char* filename, int line_number, const char* message, ...)
    __attribute__((noreturn));

#define PBL_ASSERT(expr, ...) \
  do { \
    if (!(expr)) { \
      passert_failed(__FILE_NAME__, __LINE__, __VA_ARGS__); \
    } \
  } while (0)

#define PBL_ASSERTN(expr) \
  do { \
    if (!(expr)) { \
      passert_failed_no_message(__FILE_NAME__, __LINE__); \
    } \
  } while (0)

void passert_failed_no_message(const char* filename, int line_number)
    __attribute__((noreturn));

void wtf(void) __attribute__((noreturn));

#define WTF wtf()

// Insert a compiled-in breakpoint
#define BREAKPOINT __asm("bkpt")

#define PBL_ASSERT_PRIVILEGED()
#define PBL_ASSERT_TASK(task)

#define PBL_CROAK(fmt, args...) \
    passert_failed(__FILE_NAME__, __LINE__, "*** CROAK: " fmt, ## args)
