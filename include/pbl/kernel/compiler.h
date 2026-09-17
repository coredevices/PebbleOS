/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(__clang__)
#include "pbl/kernel/compiler/clang.h"
#elif defined(__GNUC__)
#include "pbl/kernel/compiler/gcc.h"
#else
#error "Unsupported compiler"
#endif

//! Inline the function at every call site. Expands to the inline keyword as well.
#define PBL_ALWAYS_INLINE PBL_ALWAYS_INLINE_IMPL

//! Never inline the function.
#define PBL_NOINLINE PBL_NOINLINE_IMPL

//! The function does not return. Combine with the void return type.
#define PBL_NORETURN PBL_NORETURN_IMPL

//! The function has no prologue/epilogue; its body must be pure assembly.
#define PBL_NAKED PBL_NAKED_IMPL

//! Warn on every use of the symbol.
#define PBL_DEPRECATED PBL_DEPRECATED_IMPL

//! The function's result depends only on its arguments and it has no side effects.
#define PBL_CONST_FUNC PBL_CONST_FUNC_IMPL

//! Like PBL_CONST_FUNC, but the function may also read global memory.
#define PBL_PURE_FUNC PBL_PURE_FUNC_IMPL

//! Compile the function at the given optimisation level, where supported.
#define PBL_OPTIMIZE(level) PBL_OPTIMIZE_IMPL(level)

//! Check printf-style arguments: fmt is the 1-based index of the format string, va of the first
//! variadic argument.
#define PBL_FORMAT_PRINTF(fmt, va) PBL_FORMAT_PRINTF_IMPL(fmt, va)

//! Call func with a pointer to the variable when it goes out of scope.
#define PBL_CLEANUP(func) PBL_CLEANUP_IMPL(func)

//! Lay out the struct/union without padding.
#define PBL_PACKED PBL_PACKED_IMPL

//! Align the symbol or type to the given number of bytes.
#define PBL_ALIGNED(bytes) PBL_ALIGNED_IMPL(bytes)

//! Keep the symbol even if it appears unreferenced.
#define PBL_USED PBL_USED_IMPL

//! Do not warn if the symbol is unreferenced.
#define PBL_UNUSED PBL_UNUSED_IMPL

//! Emit a weak symbol, overridable by a strong definition elsewhere.
#define PBL_WEAK PBL_WEAK_IMPL

//! Make the symbol an alias of sym (a string). Combine with PBL_WEAK for a default handler.
#define PBL_ALIAS(sym) PBL_ALIAS_IMPL(sym)

//! Keep the symbol visible to code outside the compilation unit under LTO/whole-program builds.
#define PBL_EXTERNALLY_VISIBLE PBL_EXTERNALLY_VISIBLE_IMPL

//! Emit the definition as a strong symbol instead of a common one.
#define PBL_NOCOMMON PBL_NOCOMMON_IMPL

//! Place the symbol in the named linker section. A no-op in unit tests, whose host object format
//! rejects the firmware's section names.
#if UNITTEST
#define PBL_SECTION(name)
#else
#define PBL_SECTION(name) PBL_SECTION_IMPL(name)
#endif

//! Branch prediction hints for a condition that is (un)likely to hold.
#define PBL_LIKELY(x)   PBL_LIKELY_IMPL(x)
#define PBL_UNLIKELY(x) PBL_UNLIKELY_IMPL(x)

//! Mark a code path the compiler may assume is never reached.
#define PBL_UNREACHABLE() PBL_UNREACHABLE_IMPL()

//! Return address of the current function (level 0) or of its callers.
#define PBL_RETURN_ADDRESS(level) PBL_RETURN_ADDRESS_IMPL(level)

//! Constant expression: 1 if the two types are compatible, else 0.
#define PBL_TYPES_COMPATIBLE(a, b) PBL_TYPES_COMPATIBLE_IMPL(a, b)

//! Pick expression a or b from a constant condition without type-converting the other.
#define PBL_CHOOSE_EXPR(cond, a, b) PBL_CHOOSE_EXPR_IMPL(cond, a, b)

//! Leading zero count of a non-zero unsigned int.
#define PBL_CLZ(x) PBL_CLZ_IMPL(x)

//! Number of set bits in an unsigned int.
#define PBL_POPCOUNT(x) PBL_POPCOUNT_IMPL(x)

//! Byte-swap a 16/32-bit value.
#define PBL_BSWAP16(x) PBL_BSWAP16_IMPL(x)
#define PBL_BSWAP32(x) PBL_BSWAP32_IMPL(x)
