/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

#include <syscall/syscall_internal.h>

//! Largest argument PBL_SYSCALL copies onto the syscall stack.
#define PBL_SYSCALL_COPY_MAX 128

//! Define a syscall whose arguments are checked from their declared kinds.
//!
//! Each argument is a tuple (KIND, type, name[, extra]):
//! - (VAL, type, name): passed through.
//! - (IN, const T *, name): copied in before the call; the handler gets a
//!   kernel copy.
//! - (OUT, T *, name) / (INOUT, T *, name): copied in, then copied back
//!   out after the handler returns, so bytes it does not write are kept.
//! - IN_OPT, OUT_OPT, INOUT_OPT: as above, but may be nullptr.
//! - (IN_BUF, const void *, name, len) / (OUT_BUF, void *, name, len):
//!   [name, name + len) must be in the caller's memory; not copied.
//! - (IN_ARRAY, const T *, name, count) / (OUT_ARRAY, T *, name, count): as
//!   IN_BUF/OUT_BUF for count elements; an overflowing size is rejected.
//! - (STR, const char *, name): NUL-terminated within the caller's memory;
//!   not copied.
//! - (STRN, const char *, name, max): as STR, at most max characters long.
//!
//! An unprivileged caller passing an argument that fails its check is killed.
//! Privileged callers skip all checks. The handler body follows the macro,
//! like a function body, and also gets bool from_user, true for a checked call
//! from unprivileged code:
//!
//!   PBL_SYSCALL(int, sys_foo, (VAL, int, a), (OUT, int *, b)) {
//!     *b = a;
//!     return 0;
//!   }
#define PBL_SYSCALL(ret, name, ...) PRV_SC_DEFINE(, return, ret, name, __VA_ARGS__)

//! As PBL_SYSCALL, for a syscall that never returns.
#define PBL_SYSCALL_NORETURN(name, ...) PRV_SC_DEFINE([[noreturn]], , void, name, __VA_ARGS__)

#define PRV_SC_DEFINE(attrs, ret_kw, ret, name, ...)                                               \
  attrs static ret name##__impl(PRV_SC_IMPL_PARAMS(__VA_ARGS__));                                  \
  PRV_DEFINE_SYSCALL(".syscall_checked.", ret, name, PRV_SC_PARAMS(__VA_ARGS__)) {                 \
    if (!PRIVILEGE_WAS_ELEVATED) {                                                                 \
      ret_kw name##__impl(false __VA_OPT__(, PRV_SC_MAP(PRV_SC_NAME, PRV_SC_COMMA, __VA_ARGS__))); \
    }                                                                                              \
    PRV_SC_MAP(PRV_SC_CHECK, PRV_SC_EMPTY, __VA_ARGS__)                                            \
    ret_kw name##__impl(true __VA_OPT__(, PRV_SC_MAP(PRV_SC_KARG, PRV_SC_COMMA, __VA_ARGS__)));    \
  }                                                                                                \
  attrs static ret name##__impl(PRV_SC_IMPL_PARAMS(__VA_ARGS__))

//! Copy-out record run as a cleanup once the handler has returned.
struct syscall_copy_out {
  void *dst;
  const void *src;
  size_t num_bytes;
};

//! Copy @p out back to the caller, or kill it if that fails.
void syscall_copy_out(struct syscall_copy_out *out);

// Argument kinds: what the stub checks, and what the handler receives.

#define PRV_SC_CHECK(kind, type, name, ...) PRV_SC_CHECK_##kind(type, name, __VA_ARGS__)
#define PRV_SC_KARG(kind, type, name, ...)  PRV_SC_KARG_##kind(name)

#define PRV_SC_CHECK_VAL(type, name, ...)
#define PRV_SC_KARG_VAL(name) name

#define PRV_SC_COPY_IN(name)                                                                    \
  typeof_unqual(*(name)) name##__k;                                                             \
  static_assert(sizeof(name##__k) <= PBL_SYSCALL_COPY_MAX,                                      \
                "syscall argument " #name " is too large to copy");                             \
  if ((name) != nullptr && syscall_copy_from_user(&name##__k, (name), sizeof(name##__k)) < 0) { \
    syscall_failed();                                                                           \
  }

#define PRV_SC_COPY_OUT(name)                                                \
  struct syscall_copy_out name##__out [[gnu::cleanup(syscall_copy_out)]] = { \
    .dst = (name),                                                           \
    .src = &name##__k,                                                       \
    .num_bytes = sizeof(name##__k),                                          \
  };

#define PRV_SC_REQUIRED(name) \
  if ((name) == nullptr) {    \
    syscall_failed();         \
  }

#define PRV_SC_CHECK_IN(type, name, ...) PRV_SC_REQUIRED(name) PRV_SC_COPY_IN(name)
#define PRV_SC_KARG_IN(name)             (&name##__k)

#define PRV_SC_CHECK_IN_OPT(type, name, ...) PRV_SC_COPY_IN(name)
#define PRV_SC_KARG_IN_OPT(name)             ((name) != nullptr ? &name##__k : nullptr)

#define PRV_SC_CHECK_OUT(type, name, ...) \
  PRV_SC_REQUIRED(name) PRV_SC_COPY_IN(name) PRV_SC_COPY_OUT(name)
#define PRV_SC_KARG_OUT(name) (&name##__k)

#define PRV_SC_CHECK_OUT_OPT(type, name, ...) PRV_SC_COPY_IN(name) PRV_SC_COPY_OUT(name)
#define PRV_SC_KARG_OUT_OPT(name)             ((name) != nullptr ? &name##__k : nullptr)

#define PRV_SC_CHECK_INOUT     PRV_SC_CHECK_OUT
#define PRV_SC_KARG_INOUT      PRV_SC_KARG_OUT
#define PRV_SC_CHECK_INOUT_OPT PRV_SC_CHECK_OUT_OPT
#define PRV_SC_KARG_INOUT_OPT  PRV_SC_KARG_OUT_OPT

#define PRV_SC_CHECK_IN_BUF(type, name, len) syscall_assert_userspace_buffer((name), (len));
#define PRV_SC_KARG_IN_BUF(name)             name

#define PRV_SC_CHECK_OUT_BUF PRV_SC_CHECK_IN_BUF
#define PRV_SC_KARG_OUT_BUF  PRV_SC_KARG_IN_BUF

#define PRV_SC_CHECK_IN_ARRAY(type, name, count)               \
  size_t name##__n;                                            \
  if (ckd_mul(&name##__n, (size_t)(count), sizeof(*(name)))) { \
    syscall_failed();                                          \
  }                                                            \
  syscall_assert_userspace_buffer((name), name##__n);
#define PRV_SC_KARG_IN_ARRAY(name) name

#define PRV_SC_CHECK_OUT_ARRAY PRV_SC_CHECK_IN_ARRAY
#define PRV_SC_KARG_OUT_ARRAY  PRV_SC_KARG_IN_ARRAY

#define PRV_SC_CHECK_STRN(type, name, max)       \
  if (syscall_user_strnlen((name), (max)) < 0) { \
    syscall_failed();                            \
  }
#define PRV_SC_KARG_STRN(name) name

#define PRV_SC_CHECK_STR(type, name, ...) PRV_SC_CHECK_STRN(type, name, SIZE_MAX)
#define PRV_SC_KARG_STR(name)             name

// Argument list helpers.

#define PRV_SC_PARAM(kind, type, name, ...) type name
#define PRV_SC_NAME(kind, type, name, ...)  name

#define PRV_SC_PARAMS(...)   PRV_SC_CAT(PRV_SC_PARAMS_, PRV_SC_HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)
#define PRV_SC_PARAMS_0(...) void
#define PRV_SC_PARAMS_1(...) PRV_SC_MAP(PRV_SC_PARAM, PRV_SC_COMMA, __VA_ARGS__)

#define PRV_SC_IMPL_PARAMS(...) \
  bool from_user __VA_OPT__(, PRV_SC_MAP(PRV_SC_PARAM, PRV_SC_COMMA, __VA_ARGS__))

#define PRV_SC_COMMA() ,
#define PRV_SC_EMPTY()

#define PRV_SC_CAT(a, b)  PRV_SC_CAT_(a, b)
#define PRV_SC_CAT_(a, b) a##b

#define PRV_SC_HAS_ARGS(...)     PRV_SC_HAS_ARGS_(__VA_OPT__(1, ) 0, )
#define PRV_SC_HAS_ARGS_(x, ...) x

#define PRV_SC_NARGS(...) PRV_SC_NARGS_(__VA_ARGS__ __VA_OPT__(, ) 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define PRV_SC_NARGS_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, n, ...) n

#define PRV_SC_MAP(f, sep, ...) \
  PRV_SC_CAT(PRV_SC_MAP_, PRV_SC_NARGS(__VA_ARGS__))(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_0(f, sep, ...)
#define PRV_SC_MAP_1(f, sep, a)       f a
#define PRV_SC_MAP_2(f, sep, a, ...)  f a sep() PRV_SC_MAP_1(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_3(f, sep, a, ...)  f a sep() PRV_SC_MAP_2(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_4(f, sep, a, ...)  f a sep() PRV_SC_MAP_3(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_5(f, sep, a, ...)  f a sep() PRV_SC_MAP_4(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_6(f, sep, a, ...)  f a sep() PRV_SC_MAP_5(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_7(f, sep, a, ...)  f a sep() PRV_SC_MAP_6(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_8(f, sep, a, ...)  f a sep() PRV_SC_MAP_7(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_9(f, sep, a, ...)  f a sep() PRV_SC_MAP_8(f, sep, __VA_ARGS__)
#define PRV_SC_MAP_10(f, sep, a, ...) f a sep() PRV_SC_MAP_9(f, sep, __VA_ARGS__)
