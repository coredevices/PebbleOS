/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup kernel_syscall_desc Syscall descriptors
 * @ingroup kernel_syscall
 * @brief What each PBL_SYSCALL() takes, for tools that call syscalls generically.
 *
 * With @c CONFIG_SYSCALL_DESC, every PBL_SYSCALL() also emits a struct pbl_syscall_desc into
 * the @c .syscall_desc section, between @c __syscall_desc_start__ and @c __syscall_desc_end__.
 * @{
 */

/** @brief Argument kinds of PBL_SYSCALL(). */
enum pbl_syscall_arg_kind {
  PBL_SYSCALL_ARG_VAL,
  PBL_SYSCALL_ARG_IN,
  PBL_SYSCALL_ARG_IN_OPT,
  PBL_SYSCALL_ARG_OUT,
  PBL_SYSCALL_ARG_OUT_OPT,
  PBL_SYSCALL_ARG_INOUT,
  PBL_SYSCALL_ARG_INOUT_OPT,
  PBL_SYSCALL_ARG_IN_BUF,
  PBL_SYSCALL_ARG_OUT_BUF,
  PBL_SYSCALL_ARG_IN_ARRAY,
  PBL_SYSCALL_ARG_OUT_ARRAY,
  PBL_SYSCALL_ARG_STR,
  PBL_SYSCALL_ARG_STRN,
};

/** @brief Type classes, as __builtin_classify_type() reports them. */
enum pbl_syscall_type_class {
  PBL_SYSCALL_TYPE_VOID = 0,
  PBL_SYSCALL_TYPE_INTEGER = 1,
  PBL_SYSCALL_TYPE_ENUM = 3,
  PBL_SYSCALL_TYPE_BOOL = 4,
  PBL_SYSCALL_TYPE_POINTER = 5,
  PBL_SYSCALL_TYPE_REAL = 8,
  PBL_SYSCALL_TYPE_RECORD = 12,
  PBL_SYSCALL_TYPE_UNION = 13,
};

/** @brief One syscall argument. */
struct pbl_syscall_arg_desc {
  /** Argument name. */
  const char *name;
  /** The kind's extra argument (length, count or maximum), as written. */
  const char *extra;
  /** An enum pbl_syscall_arg_kind. */
  uint8_t kind;
  /** An enum pbl_syscall_type_class. */
  uint8_t type_class;
  /** Size of the argument in bytes. */
  uint8_t size;
  /** Size of the element pointed to, for pointer kinds. */
  uint16_t elem_size;
};

/** @brief The syscall never returns. */
#define PBL_SYSCALL_DESC_NORETURN (1 << 0)

/** @brief Largest argument a pbl_syscall_desc_call_fn reads from one entry of its array. */
#define PBL_SYSCALL_DESC_ARG_MAX 16

/**
 * @brief Calls the syscall and discards its result.
 *
 * @param args args[i] points at the value of argument i, aligned to 8 bytes.
 */
typedef void (*pbl_syscall_desc_call_fn)(const void *const *args);

/** @brief Description of a syscall defined with PBL_SYSCALL(). */
struct pbl_syscall_desc {
  /** Syscall name. */
  const char *name;
  /** Calls the syscall with its arguments read from an array. */
  pbl_syscall_desc_call_fn call;
  /** Its arguments, num_args of them. */
  const struct pbl_syscall_arg_desc *args;
  /** Number of arguments. */
  uint8_t num_args;
  /** PBL_SYSCALL_DESC_* flags. */
  uint8_t flags;
};

/** @brief First descriptor in the image. */
extern const struct pbl_syscall_desc __syscall_desc_start__[];
/** @brief End of the descriptors in the image. */
extern const struct pbl_syscall_desc __syscall_desc_end__[];

/** @} */
