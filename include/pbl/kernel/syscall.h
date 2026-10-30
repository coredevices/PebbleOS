/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/syscall_desc.h>

/**
 * @defgroup kernel_syscall Syscalls
 * @ingroup kernel
 * @brief Calls from unprivileged threads into privileged code.
 *
 * A syscall is defined with PBL_SYSCALL(). Its stub raises privilege, checks the arguments
 * against the kinds they are declared with, runs the body and drops privilege again on return.
 * Only stubs linked between @c __syscall_text_start__ and @c __syscall_text_end__ may raise
 * privilege. A thread created with a syscall stack runs the bodies there instead of on its own
 * stack.
 *
 * Pointers from the caller are only accepted within the thread's user memory (see
 * @ref pbl_thread_attr::user_start), above the stack pointer it made the syscall from.
 *
 * @code{.c}
 * PBL_SYSCALL(int, sys_foo, (VAL, int, a), (OUT, int *, b)) {
 *   *b = a;
 *   return 0;
 * }
 * @endcode
 * @{
 */

/** @brief Largest argument PBL_SYSCALL() copies onto the syscall stack. */
#define PBL_SYSCALL_COPY_MAX 128

/**
 * @brief Define a syscall whose arguments are checked from their declared kinds.
 *
 * Each argument is a tuple (KIND, type, name[, extra]):
 * - (VAL, type, name): passed through.
 * - (IN, const T *, name): copied in before the call; the body gets a kernel copy.
 * - (OUT, T *, name) / (INOUT, T *, name): copied in, then copied back out after the body
 *   returns, so bytes it does not write are kept.
 * - IN_OPT, OUT_OPT, INOUT_OPT: as above, but may be nullptr.
 * - (IN_BUF, const void *, name, len) / (OUT_BUF, void *, name, len): [name, name + len) must be
 *   in the caller's memory; not copied.
 * - (IN_ARRAY, const T *, name, count) / (OUT_ARRAY, T *, name, count): as IN_BUF/OUT_BUF for
 *   count elements; an overflowing size is rejected.
 * - (STR, const char *, name): NUL-terminated within the caller's memory; not copied.
 * - (STRN, const char *, name, max): as STR, at most max characters long.
 *
 * An unprivileged caller passing an argument that fails its check is killed. Privileged callers
 * skip all checks. The body follows the macro, like a function body, and also gets
 * <tt>bool from_user</tt>, true for a checked call from unprivileged code.
 *
 * @param ret Return type.
 * @param name Syscall name.
 */
#define PBL_SYSCALL(ret, name, ...) PRV_SC_DEFINE(, return, 0, ret, name, __VA_ARGS__)

/** @brief As PBL_SYSCALL(), for a syscall that never returns. */
#define PBL_SYSCALL_NORETURN(name, ...) \
  PRV_SC_DEFINE([[noreturn]], , PBL_SYSCALL_DESC_NORETURN, void, name, __VA_ARGS__)

/**
 * @brief Kill the thread whose syscall is in progress.
 *
 * For a syscall handed bad arguments. Calls pbl_kernel_syscall_fault().
 */
[[noreturn]] void pbl_syscall_fail(void);

/**
 * @brief Kill the caller unless a range is its memory.
 *
 * @param buf Start of the range.
 * @param num_bytes Length of the range.
 */
void pbl_syscall_verify_user(const void *buf, size_t num_bytes);

/**
 * @brief Check that the caller of the syscall in progress may access a range.
 *
 * @param buf Start of the range.
 * @param num_bytes Length of the range.
 * @return true if [@p buf, @p buf + @p num_bytes) is the caller's memory.
 */
bool pbl_user_access_ok(const void *buf, size_t num_bytes);

/**
 * @brief Copy from the caller's memory.
 *
 * @param dst Kernel destination.
 * @param src Source in the caller's memory.
 * @param num_bytes Bytes to copy.
 * @return 0, or -EFAULT if @p src is not the caller's memory.
 */
int pbl_user_copy_from(void *dst, const void *src, size_t num_bytes);

/**
 * @brief Copy to the caller's memory.
 *
 * @param dst Destination in the caller's memory.
 * @param src Kernel source.
 * @param num_bytes Bytes to copy.
 * @return 0, or -EFAULT if @p dst is not the caller's memory.
 */
int pbl_user_copy_to(void *dst, const void *src, size_t num_bytes);

/**
 * @brief Measure a string in the caller's memory.
 *
 * @param str String.
 * @param max Longest length accepted.
 * @return The length of @p str, or -EFAULT if it is longer than @p max or does not end within the
 * caller's memory.
 */
int pbl_user_strnlen(const char *str, size_t max);

/**
 * @brief Check whether the CPU runs unprivileged code.
 *
 * @return true in an unprivileged thread, false in a privileged one or an exception handler, and
 * always false without @c CONFIG_USERSPACE.
 */
static inline bool pbl_is_user_context(void);

#if defined(CONFIG_USERSPACE) || defined(__DOXYGEN__)
/**
 * @brief Drop the calling thread to unprivileged mode for good.
 *
 * From then on it reaches privileged code only through syscalls.
 */
void pbl_user_mode_enter(void);

/**
 * @brief Call a function unprivileged from privileged thread mode.
 *
 * For untrusted callbacks: memory or peripheral access the thread is not allowed inside @p fn
 * faults the MPU instead of silently succeeding. The caller is privileged again when this returns.
 *
 * @param fn Function to call.
 * @param ctx Argument passed to @p fn.
 */
void pbl_user_call(void (*fn)(void *), void *ctx);

/**
 * @brief Abandon the syscall in progress and resume its caller at @p fn.
 *
 * @p fn runs unprivileged, on the stack the syscall was made from, and must not return. For test
 * tools that pass syscalls bad arguments on purpose and recover from the rejections. Must be
 * called from a syscall made from unprivileged code.
 *
 * @param fn Function to resume the caller at.
 */
[[noreturn]] void pbl_syscall_abort_to(void (*fn)(void));
#endif

/**
 * @brief Called on the way out of every syscall made from unprivileged code.
 *
 * Implemented by the application. Runs privileged, on the syscall's stack, just before privilege
 * is dropped; it may block, e.g. to hold a thread that is being killed.
 */
void pbl_kernel_syscall_exit(void);

/**
 * @brief Kill the thread whose syscall failed.
 *
 * Implemented by the application; called by pbl_syscall_fail().
 *
 * @param lr Return address of the failed check, for the crash report.
 */
[[noreturn]] void pbl_kernel_syscall_fault(uintptr_t lr);

/** @} */

/** @cond INTERNAL_HIDDEN */

#if defined(UNITTEST)

static inline bool pbl_is_user_context(void) {
  return false;
}

#define PRV_SC_STUB(section, ret, name, ...) ret name(__VA_ARGS__)
#if UNITTEST_WITH_SYSCALL_PRIVILEGES
bool arch_syscall_returns_to_user(void *ret_addr);
#define PRV_SC_FROM_USER() arch_syscall_returns_to_user(PBL_RETURN_ADDRESS(0))
#else
#define PRV_SC_FROM_USER() false
#endif

#elifdef CONFIG_USERSPACE

#include <pbl_arch_syscall.h>

static inline bool pbl_is_user_context(void) {
  return arch_is_user_context();
}

#define PRV_SC_STUB      ARCH_SYSCALL_DEFINE
#define PRV_SC_FROM_USER ARCH_SYSCALL_FROM_USER

#else

static inline bool pbl_is_user_context(void) {
  return false;
}

#define PRV_SC_STUB(section, ret, name, ...) ret name(__VA_ARGS__)
#define PRV_SC_FROM_USER()                   false

#endif

#define PRV_SC_DEFINE(attrs, ret_kw, flags, ret, name, ...)                                        \
  attrs static ret name##__impl(PRV_SC_IMPL_PARAMS(__VA_ARGS__));                                  \
  PRV_SC_STUB(".syscall_checked.", ret, name, PRV_SC_PARAMS(__VA_ARGS__)) {                        \
    if (!PRV_SC_FROM_USER()) {                                                                     \
      ret_kw name##__impl(false __VA_OPT__(, PRV_SC_MAP(PRV_SC_NAME, PRV_SC_COMMA, __VA_ARGS__))); \
    }                                                                                              \
    PRV_SC_MAP(PRV_SC_CHECK, PRV_SC_EMPTY, __VA_ARGS__)                                            \
    ret_kw name##__impl(true __VA_OPT__(, PRV_SC_MAP(PRV_SC_KARG, PRV_SC_COMMA, __VA_ARGS__)));    \
  }                                                                                                \
  PRV_SC_DESC(flags, ret, name, __VA_ARGS__)                                                       \
  attrs static ret name##__impl(PRV_SC_IMPL_PARAMS(__VA_ARGS__))

#ifdef CONFIG_SYSCALL_DESC
#define PRV_SC_DESC(desc_flags, ret, sc, ...)                                                      \
  static void sc##__call(const void *const *prv_sc_args) {                                         \
    (void)prv_sc_args;                                                                             \
    PRV_SC_MAP(PRV_SC_LOAD_ARG, PRV_SC_EMPTY, __VA_ARGS__)                                         \
    (void)sc(__VA_OPT__(PRV_SC_MAP(PRV_SC_NAME, PRV_SC_COMMA, __VA_ARGS__)));                      \
  }                                                                                                \
  [[gnu::used, gnu::section(".syscall_desc")]] static const struct pbl_syscall_desc sc##__desc = { \
    .name = #sc,                                                                                   \
    .call = sc##__call,                                                                            \
    .args = PRV_SC_CAT(PRV_SC_ARG_DESCS_, PRV_SC_HAS_ARGS(__VA_ARGS__))(__VA_ARGS__),              \
    .num_args = PRV_SC_NARGS(__VA_ARGS__),                                                         \
    .flags = (desc_flags),                                                                         \
  };

#define PRV_SC_LOAD_ARG(kind, type, arg, ...)                                           \
  typeof_unqual(type) arg;                                                              \
  static_assert(sizeof(arg) <= PBL_SYSCALL_DESC_ARG_MAX, "syscall argument too large"); \
  __builtin_memcpy(&arg, *prv_sc_args++, sizeof(arg));

#define PRV_SC_ARG_DESCS_0(...) nullptr
#define PRV_SC_ARG_DESCS_1(...)                            \
  (const struct pbl_syscall_arg_desc[]) {                  \
    PRV_SC_MAP(PRV_SC_ARG_DESC, PRV_SC_COMMA, __VA_ARGS__) \
  }

#define PRV_SC_ARG_DESC(arg_kind, type, arg, ...) \
  {                                               \
    .name = #arg,                                 \
    .extra = "" #__VA_ARGS__,                     \
    .kind = PBL_SYSCALL_ARG_##arg_kind,           \
    .type_class = __builtin_classify_type(type),  \
    .size = sizeof(type),                         \
    .elem_size = PRV_SC_ELEM_##arg_kind(type),    \
  }

#define PRV_SC_ELEM_PTR(type)  sizeof(*(type) nullptr)
#define PRV_SC_ELEM_BYTE(type) 1
#define PRV_SC_ELEM_VAL        PRV_SC_ELEM_BYTE
#define PRV_SC_ELEM_IN         PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_IN_OPT     PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_OUT        PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_OUT_OPT    PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_INOUT      PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_INOUT_OPT  PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_IN_BUF     PRV_SC_ELEM_BYTE
#define PRV_SC_ELEM_OUT_BUF    PRV_SC_ELEM_BYTE
#define PRV_SC_ELEM_IN_ARRAY   PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_OUT_ARRAY  PRV_SC_ELEM_PTR
#define PRV_SC_ELEM_STR        PRV_SC_ELEM_BYTE
#define PRV_SC_ELEM_STRN       PRV_SC_ELEM_BYTE
#else
#define PRV_SC_DESC(flags, ret, name, ...)
#endif

//! Copy-out record run as a cleanup once the body has returned.
struct pbl_syscall_copy_out {
  void *dst;
  const void *src;
  size_t num_bytes;
};

//! Copy @p out back to the caller, or kill it if that fails.
void pbl_syscall_copy_out(struct pbl_syscall_copy_out *out);

// Argument kinds: what the stub checks, and what the body receives.

#define PRV_SC_CHECK(kind, type, name, ...) PRV_SC_CHECK_##kind(type, name, __VA_ARGS__)
#define PRV_SC_KARG(kind, type, name, ...)  PRV_SC_KARG_##kind(name)

#define PRV_SC_CHECK_VAL(type, name, ...)
#define PRV_SC_KARG_VAL(name) name

#define PRV_SC_COPY_IN(name)                                                                \
  typeof_unqual(*(name)) name##__k;                                                         \
  static_assert(sizeof(name##__k) <= PBL_SYSCALL_COPY_MAX,                                  \
                "syscall argument " #name " is too large to copy");                         \
  if ((name) != nullptr && pbl_user_copy_from(&name##__k, (name), sizeof(name##__k)) < 0) { \
    pbl_syscall_fail();                                                                     \
  }

#define PRV_SC_COPY_OUT(name)                                                        \
  struct pbl_syscall_copy_out name##__out [[gnu::cleanup(pbl_syscall_copy_out)]] = { \
    .dst = (name),                                                                   \
    .src = &name##__k,                                                               \
    .num_bytes = sizeof(name##__k),                                                  \
  };

#define PRV_SC_REQUIRED(name) \
  if ((name) == nullptr) {    \
    pbl_syscall_fail();       \
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

#define PRV_SC_CHECK_IN_BUF(type, name, len) pbl_syscall_verify_user((name), (len));
#define PRV_SC_KARG_IN_BUF(name)             name

#define PRV_SC_CHECK_OUT_BUF PRV_SC_CHECK_IN_BUF
#define PRV_SC_KARG_OUT_BUF  PRV_SC_KARG_IN_BUF

#define PRV_SC_CHECK_IN_ARRAY(type, name, count)               \
  size_t name##__n;                                            \
  if (ckd_mul(&name##__n, (size_t)(count), sizeof(*(name)))) { \
    pbl_syscall_fail();                                        \
  }                                                            \
  pbl_syscall_verify_user((name), name##__n);
#define PRV_SC_KARG_IN_ARRAY(name) name

#define PRV_SC_CHECK_OUT_ARRAY PRV_SC_CHECK_IN_ARRAY
#define PRV_SC_KARG_OUT_ARRAY  PRV_SC_KARG_IN_ARRAY

#define PRV_SC_CHECK_STRN(type, name, max)   \
  if (pbl_user_strnlen((name), (max)) < 0) { \
    pbl_syscall_fail();                      \
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

/** @endcond */
