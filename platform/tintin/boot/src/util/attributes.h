/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(__clang__)
#define GCC_ONLY(x)
#else
#define GCC_ONLY(x) x
#endif

// Function attributes
#define FORMAT_FUNC(TYPE, STR_IDX, FIRST) __attribute__((__format__(TYPE, STR_IDX, FIRST)))

#define FORMAT_PRINTF(STR_IDX, FIRST) FORMAT_FUNC(__printf__, STR_IDX, FIRST)

#define ALWAYS_INLINE __attribute__((__always_inline__)) inline
#define NOINLINE __attribute__((__noinline__))
#define NORETURN __attribute__((__noreturn__)) void
#define NAKED_FUNC __attribute__((__naked__))
#define OPTIMIZE_FUNC(LVL) __attribute__((__optimize__(LVL)))
#define CONST_FUNC __attribute__((__const__))
#define PURE_FUNC __attribute__((__pure__))

// Variable attributes
#define ATTR_CLEANUP(FUNC) __attribute__((__cleanup__(FUNC)))

// Structure attributes
#define PACKED __attribute__((__packed__))

// General attributes
#define USED __attribute__((__used__))
#define UNUSED __attribute__((__unused__))
#define WEAK __attribute__((__weak__))
#define ALIAS(sym) __attribute__((__weak__, __alias__(sym)))
#define SECTION(SEC) GCC_ONLY(__attribute__((__section__(SEC))))
#define EXTERNALLY_VISIBLE GCC_ONLY(__attribute__((__externally_visible__)))
