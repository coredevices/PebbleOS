/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Argument kinds of PBL_SYSCALL, as recorded in a syscall_desc.
enum syscall_arg_kind {
  SYSCALL_ARG_VAL,
  SYSCALL_ARG_IN,
  SYSCALL_ARG_IN_OPT,
  SYSCALL_ARG_OUT,
  SYSCALL_ARG_OUT_OPT,
  SYSCALL_ARG_INOUT,
  SYSCALL_ARG_INOUT_OPT,
  SYSCALL_ARG_IN_BUF,
  SYSCALL_ARG_OUT_BUF,
  SYSCALL_ARG_IN_ARRAY,
  SYSCALL_ARG_OUT_ARRAY,
  SYSCALL_ARG_STR,
  SYSCALL_ARG_STRN,
};

//! Type classes, as __builtin_classify_type() reports them.
enum syscall_type_class {
  SYSCALL_TYPE_VOID = 0,
  SYSCALL_TYPE_INTEGER = 1,
  SYSCALL_TYPE_ENUM = 3,
  SYSCALL_TYPE_BOOL = 4,
  SYSCALL_TYPE_POINTER = 5,
  SYSCALL_TYPE_REAL = 8,
  SYSCALL_TYPE_RECORD = 12,
  SYSCALL_TYPE_UNION = 13,
};

struct syscall_arg_desc {
  const char *name;
  //! The kind's extra argument (length, count or maximum), as written.
  const char *extra;
  uint8_t kind;
  uint8_t type_class;
  uint8_t size;
  //! Size of the element pointed to, for pointer kinds.
  uint16_t elem_size;
};

#define SYSCALL_DESC_NORETURN (1 << 0)

//! Largest argument a syscall_desc_call_fn reads from one entry of args.
#define SYSCALL_DESC_ARG_MAX 16

//! Calls the syscall with args[i] pointing at the value of argument i,
//! aligned to 8 bytes, and discards its result.
typedef void (*syscall_desc_call_fn)(const void *const *args);

//! Description of a syscall defined with PBL_SYSCALL, kept in .syscall_desc when
//! CONFIG_SYSCALL_FUZZ is set.
struct syscall_desc {
  const char *name;
  syscall_desc_call_fn call;
  const struct syscall_arg_desc *args;
  uint8_t num_args;
  uint8_t flags;
};

extern const struct syscall_desc __syscall_desc_start__[];
extern const struct syscall_desc __syscall_desc_end__[];
