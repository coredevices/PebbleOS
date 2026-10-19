/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/syscall.h>

// What third_party/moddable still uses of the old syscall interface.

//! Defines a syscall that checks its own arguments.
#define DEFINE_SYSCALL(retType, funcName, ...) \
  PRV_SC_STUB(".syscall_text.", retType, funcName, __VA_ARGS__)

#define syscall_failed pbl_syscall_fail
