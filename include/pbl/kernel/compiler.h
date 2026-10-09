/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(__clang__)
#include <pbl/kernel/compiler/clang.h>
#elif defined(__GNUC__)
#include <pbl/kernel/compiler/gcc.h>
#else
#error "Unsupported compiler"
#endif

/**
 * @defgroup kernel_compiler Compiler abstraction
 * @ingroup kernel
 * @brief Attributes and builtins, independent of the compiler.
 *
 * The only place the tree may spell compiler specifics: code outside @c pbl/kernel/compiler/ must
 * not use @c __attribute__ or @c __builtin_* directly. Each macro expands to a @c *_IMPL
 * counterpart from @c compiler/gcc.h or @c compiler/clang.h; attributes a compiler does not
 * implement expand to nothing.
 *
 * @code{.c}
 * typedef struct PBL_PACKED {
 *   uint16_t id;
 *   uint32_t value;
 * } Record;
 *
 * [[noreturn]] void fatal(const char *fmt, ...) PBL_FORMAT_PRINTF(1, 2);
 *
 * if (PBL_UNLIKELY(len > MAX_LEN)) {
 *   return -EINVAL;
 * }
 * @endcode
 * @{
 */

/** @brief Inline the function at every call site. Expands to the @c inline keyword as well. */
#define PBL_ALWAYS_INLINE PBL_ALWAYS_INLINE_IMPL

/** @brief Never inline the function. */
#define PBL_NOINLINE PBL_NOINLINE_IMPL

/** @brief The function has no prologue/epilogue; its body must be pure assembly. */
#define PBL_NAKED PBL_NAKED_IMPL

/** @brief Warn on every use of the symbol. */
#define PBL_DEPRECATED PBL_DEPRECATED_IMPL

/** @brief The function's result depends only on its arguments and it has no side effects. */
#define PBL_CONST_FUNC PBL_CONST_FUNC_IMPL

/** @brief Like @ref PBL_CONST_FUNC, but the function may also read global memory. */
#define PBL_PURE_FUNC PBL_PURE_FUNC_IMPL

/**
 * @brief Compile the function at the given optimization level, where supported.
 *
 * A no-op with Clang.
 *
 * @param level Level as a string, e.g. @c "O2".
 */
#define PBL_OPTIMIZE(level) PBL_OPTIMIZE_IMPL(level)

/**
 * @brief Check printf-style arguments.
 *
 * @param fmt 1-based index of the format string parameter.
 * @param va 1-based index of the first variadic parameter, 0 for a @c va_list function.
 */
#define PBL_FORMAT_PRINTF(fmt, va) PBL_FORMAT_PRINTF_IMPL(fmt, va)

/**
 * @brief Call a function with a pointer to the variable when it goes out of scope.
 *
 * @param func Function taking a pointer to the variable's type.
 */
#define PBL_CLEANUP(func) PBL_CLEANUP_IMPL(func)

/** @brief Lay out the struct/union without padding. */
#define PBL_PACKED PBL_PACKED_IMPL

/**
 * @brief Align the symbol or type.
 *
 * @param bytes Alignment in bytes, a power of two.
 */
#define PBL_ALIGNED(bytes) PBL_ALIGNED_IMPL(bytes)

/** @brief Keep the symbol even if it appears unreferenced. */
#define PBL_USED PBL_USED_IMPL

/** @brief Emit a weak symbol, overridable by a strong definition elsewhere. */
#define PBL_WEAK PBL_WEAK_IMPL

/**
 * @brief Make the symbol an alias of another one.
 *
 * Combine with @ref PBL_WEAK for an overridable default handler.
 *
 * @param sym Target symbol name, as a string.
 */
#define PBL_ALIAS(sym) PBL_ALIAS_IMPL(sym)

/**
 * @brief Keep the symbol visible to code outside the compilation unit under LTO/whole-program
 * builds.
 *
 * A no-op with Clang.
 */
#define PBL_EXTERNALLY_VISIBLE PBL_EXTERNALLY_VISIBLE_IMPL

/** @brief Emit the definition as a strong symbol instead of a common one. */
#define PBL_NOCOMMON PBL_NOCOMMON_IMPL

/**
 * @brief Place the symbol in a linker section.
 *
 * A no-op in unit tests and in builds linked without the firmware linker script
 * (@c PBL_NO_LINKER_SCRIPT), where nothing would gather the sections.
 *
 * @param name Section name, as a string.
 */
#if UNITTEST || defined(PBL_NO_LINKER_SCRIPT)
#define PBL_SECTION(name)
#else
#define PBL_SECTION(name) PBL_SECTION_IMPL(name)
#endif

/**
 * @brief Branch prediction hint for a condition that is likely to hold.
 *
 * @param x Condition.
 */
#define PBL_LIKELY(x) PBL_LIKELY_IMPL(x)
/**
 * @brief Branch prediction hint for a condition that is unlikely to hold.
 *
 * @param x Condition.
 */
#define PBL_UNLIKELY(x) PBL_UNLIKELY_IMPL(x)

/** @brief Mark a code path the compiler may assume is never reached. */
#define PBL_UNREACHABLE() PBL_UNREACHABLE_IMPL()

/**
 * @brief Return address of the current function or of its callers.
 *
 * @param level 0 for the current function, 1 for its caller, and so on; a constant.
 */
#define PBL_RETURN_ADDRESS(level) PBL_RETURN_ADDRESS_IMPL(level)

/**
 * @brief Constant expression: 1 if the two types are compatible, else 0.
 *
 * @param a First type.
 * @param b Second type.
 */
#define PBL_TYPES_COMPATIBLE(a, b) PBL_TYPES_COMPATIBLE_IMPL(a, b)

/**
 * @brief Pick an expression from a constant condition without type-converting the other.
 *
 * @param cond Constant condition.
 * @param a Result if @p cond is non-zero.
 * @param b Result if @p cond is zero.
 */
#define PBL_CHOOSE_EXPR(cond, a, b) PBL_CHOOSE_EXPR_IMPL(cond, a, b)

/**
 * @brief Count leading zero bits.
 *
 * @param x Unsigned int; undefined for 0.
 */
#define PBL_CLZ(x) PBL_CLZ_IMPL(x)

/**
 * @brief Count set bits.
 *
 * @param x Unsigned int.
 */
#define PBL_POPCOUNT(x) PBL_POPCOUNT_IMPL(x)

/**
 * @brief Keep AddressSanitizer off the object: no redzones around a global,
 * e.g. one that must sit next to its neighbours in a section.
 */
#define PBL_NO_SANITIZE_ADDRESS PBL_NO_SANITIZE_ADDRESS_IMPL

/**
 * @brief Add, detecting overflow. Prefer the typed helpers of pbl/util/math.h.
 *
 * @param a First operand.
 * @param b Second operand.
 * @param r Where the result goes, wrapped around on overflow.
 * @return true if the result overflowed the type of @p r.
 */
#define PBL_ADD_OVERFLOW(a, b, r) PBL_ADD_OVERFLOW_IMPL(a, b, r)

/**
 * @brief Multiply, detecting overflow. Prefer the typed helpers of pbl/util/math.h.
 *
 * @param a First operand.
 * @param b Second operand.
 * @param r Where the result goes, wrapped around on overflow.
 * @return true if the result overflowed the type of @p r.
 */
#define PBL_MUL_OVERFLOW(a, b, r) PBL_MUL_OVERFLOW_IMPL(a, b, r)

/**
 * @brief Reverse the bytes of a 16-bit value.
 *
 * @param x Value.
 */
#define PBL_BSWAP16(x) PBL_BSWAP16_IMPL(x)
/**
 * @brief Reverse the bytes of a 32-bit value.
 *
 * @param x Value.
 */
#define PBL_BSWAP32(x) PBL_BSWAP32_IMPL(x)

/** @} */
