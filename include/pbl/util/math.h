/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_math Integer math
 * @ingroup util
 * @brief Integer math macros and helpers.
 *
 * The macros evaluate their arguments more than once; do not pass expressions with side
 * effects.
 *
 * @code{.c}
 * int32_t clamped = PBL_CLAMP(value, -100, 100);
 * uint32_t pages = PBL_DIV_ROUND_UP(len, PAGE_SIZE);
 * if (PBL_IN_RANGE(c, '0', '9')) {
 *   ...
 * }
 * @endcode
 * @{
 */

/**
 * @brief Get the smaller of two values.
 *
 * @param a First value.
 * @param b Second value.
 */
#define PBL_MIN(a, b) (((a) < (b)) ? (a) : (b))

/**
 * @brief Get the larger of two values.
 *
 * @param a First value.
 * @param b Second value.
 */
#define PBL_MAX(a, b) (((a) > (b)) ? (a) : (b))

/**
 * @brief Get the absolute value.
 *
 * @param a Value.
 */
#define PBL_ABS(a) (((a) > 0) ? (a) : -1 * (a))
/**
 * @brief Clamp a value to a range.
 *
 * @param n Value.
 * @param min Lower bound.
 * @param max Upper bound.
 */
#define PBL_CLAMP(n, min, max) ((n) < (min) ? (min) : ((n) > (max) ? (max) : (n)))
/**
 * @brief Divide, rounding half up, for non-negative operands.
 *
 * @param num Numerator.
 * @param denom Denominator.
 */
#define PBL_DIV_ROUND_CLOSEST(num, denom) (((num) + ((denom) / 2)) / (denom))
/**
 * @brief Check whether a value is within a closed range.
 *
 * @param n Value.
 * @param min Lower bound, included.
 * @param max Upper bound, included.
 */
#define PBL_IN_RANGE(n, min, max) ((n) >= (min) && (n) <= (max))

/**
 * @brief Divide, rounding up for positive results.
 *
 * Negative results round towards zero: PBL_DIV_ROUND_UP(3, 4) is 1 and PBL_DIV_ROUND_UP(-3, 4) is
 * 0.
 *
 * @param num Numerator.
 * @param denom Denominator, positive.
 */
#define PBL_DIV_ROUND_UP(num, denom) (((num) + ((denom) - 1)) / (denom))

/**
 * @brief Round a value away from zero to a multiple of a modulus.
 *
 * PBL_ROUNDUP(152, 32) is 160 and PBL_ROUNDUP(-32, 90) is -90.
 *
 * @param val Value.
 * @param mod Modulus; its sign is ignored.
 */
#define PBL_ROUNDUP(val, mod)                                                           \
  (((val) >= 0) ? ((((val) + PBL_ABS(PBL_ABS(mod) - 1)) / PBL_ABS(mod)) * PBL_ABS(mod)) \
                : -((((-val) + PBL_ABS(PBL_ABS(mod) - 1)) / PBL_ABS(mod)) * PBL_ABS(mod)))

/**
 * @brief Sign-extend the low bits of a value.
 *
 * @param a Value; bits above @p bits are ignored.
 * @param bits Width of the signed value, 1 to 32.
 * @return Sign-extended value.
 */
int32_t sign_extend(uint32_t a, int bits);

/**
 * @brief Compute the distance between two 32-bit serial numbers, handling wrap-around.
 *
 * @param start Start value.
 * @param end End value.
 * @return @p end - @p start, using serial number arithmetic (RFC 1982).
 */
int32_t serial_distance32(uint32_t start, uint32_t end);

/**
 * @brief Compute the distance between two serial numbers, handling wrap-around.
 *
 * @param start Start value.
 * @param end End value.
 * @param bits Number of valid bits in @p start and @p end.
 * @return @p end - @p start, using serial number arithmetic (RFC 1982).
 */
int32_t serial_distance(uint32_t start, uint32_t end, int bits);

/**
 * @brief Compute the base 2 logarithm, rounded up.
 *
 * @param n Value, greater than 0.
 * @return ceil(log2(@p n)).
 */
int ceil_log_two(uint32_t n);

/**
 * @brief Compute the integer square root with Newton's method.
 *
 * @param x Value.
 * @return floor(sqrt(@p x)), 0 for negative values.
 */
int32_t integer_sqrt(int64_t x);

/**
 * Determine whether a variable is signed or not.
 * @param var The variable to evaluate.
 * @return true if the variable is signed.
 */
#define PBL_IS_SIGNED(var)                                                                   \
  (PBL_CHOOSE_EXPR(                                                                          \
      PBL_TYPES_COMPATIBLE(__typeof__(var), unsigned char), false,                           \
      PBL_CHOOSE_EXPR(                                                                       \
          PBL_TYPES_COMPATIBLE(__typeof__(var), unsigned short), false,                      \
          PBL_CHOOSE_EXPR(                                                                   \
              PBL_TYPES_COMPATIBLE(__typeof__(var), unsigned int), false,                    \
              PBL_CHOOSE_EXPR(                                                               \
                  PBL_TYPES_COMPATIBLE(__typeof__(var), unsigned long), false,               \
                  PBL_CHOOSE_EXPR(PBL_TYPES_COMPATIBLE(__typeof__(var), unsigned long long), \
                                  false, true))))))

/**
 * @brief Add two 32-bit unsigned integers, detecting overflow.
 *
 * @param a First operand.
 * @param b Second operand.
 * @param[out] result Sum, wrapped around on overflow.
 * @return true if the sum overflowed.
 */
static inline bool pbl_u32_add_overflow(uint32_t a, uint32_t b, uint32_t *result) {
  return ckd_add(result, a, b);
}

/**
 * @brief Multiply two 32-bit unsigned integers, detecting overflow.
 *
 * @param a First operand.
 * @param b Second operand.
 * @param[out] result Product, wrapped around on overflow.
 * @return true if the product overflowed.
 */
static inline bool pbl_u32_mul_overflow(uint32_t a, uint32_t b, uint32_t *result) {
  return ckd_mul(result, a, b);
}

/**
 * @brief Add two sizes, detecting overflow.
 *
 * @param a First operand.
 * @param b Second operand.
 * @param[out] result Sum, wrapped around on overflow.
 * @return true if the sum overflowed.
 */
static inline bool pbl_size_add_overflow(size_t a, size_t b, size_t *result) {
  return ckd_add(result, a, b);
}

/**
 * @brief Multiply two sizes, detecting overflow.
 *
 * @param a First operand.
 * @param b Second operand.
 * @param[out] result Product, wrapped around on overflow.
 * @return true if the product overflowed.
 */
static inline bool pbl_size_mul_overflow(size_t a, size_t b, size_t *result) {
  return ckd_mul(result, a, b);
}

/**
 * @brief Compute a modulo that is never negative.
 *
 * @param i Dividend.
 * @param n Divisor, positive.
 * @return @p i mod @p n, in [0, @p n).
 */
static inline int positive_modulo(int i, int n) {
  return (i % n + n) % n;
}

/**
 * @brief Compute the distance from a value to the nearest multiple of a modulus.
 *
 * For angles, the smallest difference between two angles is
 * distance_to_mod_boundary(a - b, 360).
 *
 * @param i Value.
 * @param n Modulus, positive.
 * @return Distance, 0 to @p n / 2.
 */
static inline int distance_to_mod_boundary(int32_t i, uint16_t n) {
  const int mod = positive_modulo(i, n);
  const int half = n / 2;
  return PBL_ABS((mod + half) % n - half);
}

/**
 * @brief Compute the next interval of a bounded binary exponential backoff.
 *
 * @param[in,out] attempt Retries performed so far, incremented by the call.
 * @param initial_value First interval; later ones are this multiplied by a power of 2.
 * @param max_value Maximum interval returned.
 * @return Next interval, @p initial_value * 2^@p attempt capped to @p max_value.
 */
uint32_t next_exponential_backoff(uint32_t *attempt, uint32_t initial_value, uint32_t max_value);

/**
 * @brief Compute the greatest common divisor of two numbers.
 *
 * @param a First number.
 * @param b Second number.
 * @return Greatest common divisor, 0 if either number is 0.
 */
uint32_t gcd(uint32_t a, uint32_t b);

/** @} */
