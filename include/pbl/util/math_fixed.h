/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_math_fixed Fixed-point math
 * @ingroup util
 * @brief Signed fixed-point types and a generic recursive filter.
 *
 * In every type the fraction is unsigned and adds to the integer part, so -1.125 in
 * Fixed_S16_3 is stored as integer -2 and fraction 7 (-2 + 7 * 0.125), and 1.125 as integer 1
 * and fraction 1. This keeps @c raw_value a plain two's complement number, so values can be
 * added and multiplied directly.
 *
 * @code{.c}
 * Fixed_S16_3 a = Fixed_S16_3(12);  // 1.5
 * Fixed_S16_3 b = FIXED_S16_3_ONE;
 * int16_t r = Fixed_S16_3_rounded_int(Fixed_S16_3_add(a, b));  // 3
 * @endcode
 * @{
 */

/** @brief Fixed-point number with 1 sign bit, 12 integer bits and 3 fraction bits. */
typedef union PBL_PACKED Fixed_S16_3 {
  /** Value scaled by 8. */
  int16_t raw_value;
  struct {
    /** Fraction, in eighths. */
    uint16_t fraction : 3;
    /** Integer part, rounded towards negative infinity. */
    int16_t integer : 13;
  };
} Fixed_S16_3;

/**
 * @brief Make a Fixed_S16_3 from its raw value.
 *
 * @param raw Value scaled by 8.
 */
#define Fixed_S16_3(raw) ((Fixed_S16_3){.raw_value = (raw)})
/** @brief Number of fraction bits of Fixed_S16_3. */
#define FIXED_S16_3_PRECISION 3
/** @brief Scale factor of Fixed_S16_3. */
#define FIXED_S16_3_FACTOR (1 << FIXED_S16_3_PRECISION)

/** @brief Fixed_S16_3 zero. */
#define FIXED_S16_3_ZERO ((Fixed_S16_3){.integer = 0, .fraction = 0})
/** @brief Fixed_S16_3 one. */
#define FIXED_S16_3_ONE ((Fixed_S16_3){.integer = 1, .fraction = 0})
/** @brief Fixed_S16_3 one half. */
#define FIXED_S16_3_HALF ((Fixed_S16_3){.raw_value = FIXED_S16_3_ONE.raw_value / 2})

/**
 * @brief Multiply two Fixed_S16_3 values.
 *
 * @param a First factor.
 * @param b Second factor.
 * @return Product, truncated.
 */
static __inline__ Fixed_S16_3 Fixed_S16_3_mul(Fixed_S16_3 a, Fixed_S16_3 b) {
  return Fixed_S16_3(((int32_t)a.raw_value * b.raw_value) >> FIXED_S16_3_PRECISION);
}

/**
 * @brief Add two Fixed_S16_3 values.
 *
 * @param a First term.
 * @param b Second term.
 * @return Sum.
 */
static __inline__ Fixed_S16_3 Fixed_S16_3_add(Fixed_S16_3 a, Fixed_S16_3 b) {
  return Fixed_S16_3(a.raw_value + b.raw_value);
}

/**
 * @brief Subtract two Fixed_S16_3 values.
 *
 * @param a Minuend.
 * @param b Subtrahend.
 * @return @p a - @p b.
 */
static __inline__ Fixed_S16_3 Fixed_S16_3_sub(Fixed_S16_3 a, Fixed_S16_3 b) {
  return Fixed_S16_3(a.raw_value - b.raw_value);
}

/**
 * @brief Add three Fixed_S16_3 values.
 *
 * @param a First term.
 * @param b Second term.
 * @param c Third term.
 * @return Sum.
 */
static __inline__ Fixed_S16_3 Fixed_S16_3_add3(Fixed_S16_3 a, Fixed_S16_3 b, Fixed_S16_3 c) {
  return Fixed_S16_3(a.raw_value + b.raw_value + c.raw_value);
}

/**
 * @brief Compare two Fixed_S16_3 values.
 *
 * @param a First value.
 * @param b Second value.
 * @return true if equal.
 */
static __inline__ bool Fixed_S16_3_equal(Fixed_S16_3 a, Fixed_S16_3 b) {
  return (a.raw_value == b.raw_value);
}

/**
 * @brief Round a Fixed_S16_3 value to the nearest integer, halves away from zero.
 *
 * @param a Value.
 * @return Rounded value.
 */
static __inline__ int16_t Fixed_S16_3_rounded_int(Fixed_S16_3 a) {
  const int16_t delta = a.raw_value >= 0 ? FIXED_S16_3_HALF.raw_value : -FIXED_S16_3_HALF.raw_value;
  return (a.raw_value + delta) / FIXED_S16_3_FACTOR;
}

/** @brief Fixed-point number with 1 sign bit, 15 integer bits and 16 fraction bits. */
typedef union PBL_PACKED Fixed_S32_16 {
  /** Value scaled by 65536. */
  int32_t raw_value;
  struct {
    /** Fraction, in 1/65536 units. */
    uint16_t fraction : 16;
    /** Integer part, rounded towards negative infinity. */
    int16_t integer : 16;
  };
} Fixed_S32_16;

/**
 * @brief Alias of Fixed_S32_16 for function return types.
 *
 * Avoids the function-like Fixed_S32_16() macro expanding in function pointer declarations.
 */
typedef Fixed_S32_16 Fixed_S32_16Return;

/**
 * @brief Make a Fixed_S32_16 from its raw value.
 *
 * @param raw Value scaled by 65536.
 */
#define Fixed_S32_16(raw) ((Fixed_S32_16){.raw_value = (raw)})
/** @brief Number of fraction bits of Fixed_S32_16. */
#define FIXED_S32_16_PRECISION 16

/** @brief Fixed_S32_16 one. */
#define FIXED_S32_16_ONE ((Fixed_S32_16){.integer = 1, .fraction = 0})
/** @brief Fixed_S32_16 zero. */
#define FIXED_S32_16_ZERO ((Fixed_S32_16){.integer = 0, .fraction = 0})

/**
 * @brief Multiply two Fixed_S32_16 values.
 *
 * @param a First factor.
 * @param b Second factor.
 * @return Product, truncated.
 */
static __inline__ Fixed_S32_16 Fixed_S32_16_mul(Fixed_S32_16 a, Fixed_S32_16 b) {
  Fixed_S32_16 x;

  x.raw_value =
      (int32_t)((((int64_t)a.raw_value * (int64_t)b.raw_value)) >> FIXED_S32_16_PRECISION);
  return x;
}

/**
 * @brief Add two Fixed_S32_16 values.
 *
 * @param a First term.
 * @param b Second term.
 * @return Sum.
 */
static __inline__ Fixed_S32_16 Fixed_S32_16_add(Fixed_S32_16 a, Fixed_S32_16 b) {
  return Fixed_S32_16(a.raw_value + b.raw_value);
}

/**
 * @brief Add three Fixed_S32_16 values.
 *
 * @param a First term.
 * @param b Second term.
 * @param c Third term.
 * @return Sum.
 */
static __inline__ Fixed_S32_16 Fixed_S32_16_add3(Fixed_S32_16 a, Fixed_S32_16 b, Fixed_S32_16 c) {
  return Fixed_S32_16(a.raw_value + b.raw_value + c.raw_value);
}

/**
 * @brief Subtract two Fixed_S32_16 values.
 *
 * @param a Minuend.
 * @param b Subtrahend.
 * @return @p a - @p b.
 */
static __inline__ Fixed_S32_16 Fixed_S32_16_sub(Fixed_S32_16 a, Fixed_S32_16 b) {
  return Fixed_S32_16(a.raw_value - b.raw_value);
}

/** @brief Fixed-point number with 1 sign bit, 31 integer bits and 32 fraction bits. */
typedef union PBL_PACKED Fixed_S64_32 {
  /** Value scaled by 2^32. */
  int64_t raw_value;
  struct {
    /** Fraction, in 1/2^32 units. */
    uint32_t fraction : 32;
    /** Integer part, rounded towards negative infinity. */
    int32_t integer : 32;
  };
} Fixed_S64_32;

/** @brief Number of fraction bits of Fixed_S64_32. */
#define FIXED_S64_32_PRECISION 32

/** @brief Fixed_S64_32 one. */
#define FIXED_S64_32_ONE ((Fixed_S64_32){.integer = 1, .fraction = 0})
/** @brief Fixed_S64_32 zero. */
#define FIXED_S64_32_ZERO ((Fixed_S64_32){.integer = 0, .fraction = 0})

/**
 * @brief Make a Fixed_S64_32 from its raw value.
 *
 * @param raw Value scaled by 2^32.
 */
#define FIXED_S64_32_FROM_RAW(raw) ((Fixed_S64_32){.raw_value = (raw)})
/**
 * @brief Make a Fixed_S64_32 from an integer.
 *
 * @param x Integer.
 */
#define FIXED_S64_32_FROM_INT(x) ((Fixed_S64_32){.integer = x, .fraction = 0})
/**
 * @brief Get the integer part of a Fixed_S64_32, rounded towards negative infinity.
 *
 * @param x Value.
 */
#define FIXED_S64_32_TO_INT(x) (x.integer)

/**
 * @brief Multiply two Fixed_S64_32 values.
 *
 * @param a First factor.
 * @param b Second factor.
 * @return Product.
 */
static __inline__ Fixed_S64_32 Fixed_S64_32_mul(Fixed_S64_32 a, Fixed_S64_32 b) {
  Fixed_S64_32 result;
  result.raw_value = (((uint64_t)(a.integer * b.integer)) << 32) +
                     ((((uint64_t)a.fraction) * ((uint64_t)b.fraction)) >> 32) +
                     ((a.integer) * ((uint64_t)b.fraction)) +
                     (((uint64_t)a.fraction) * (b.integer));
  return result;
}

/**
 * @brief Add two Fixed_S64_32 values.
 *
 * @param a First term.
 * @param b Second term.
 * @return Sum.
 */
static __inline__ Fixed_S64_32 Fixed_S64_32_add(Fixed_S64_32 a, Fixed_S64_32 b) {
  return FIXED_S64_32_FROM_RAW(a.raw_value + b.raw_value);
}

/**
 * @brief Add three Fixed_S64_32 values.
 *
 * @param a First term.
 * @param b Second term.
 * @param c Third term.
 * @return Sum.
 */
static __inline__ Fixed_S64_32 Fixed_S64_32_add3(Fixed_S64_32 a, Fixed_S64_32 b, Fixed_S64_32 c) {
  return FIXED_S64_32_FROM_RAW(a.raw_value + b.raw_value + c.raw_value);
}

/**
 * @brief Subtract two Fixed_S64_32 values.
 *
 * @param a Minuend.
 * @param b Subtrahend.
 * @return @p a - @p b.
 */
static __inline__ Fixed_S64_32 Fixed_S64_32_sub(Fixed_S64_32 a, Fixed_S64_32 b) {
  return FIXED_S64_32_FROM_RAW(a.raw_value - b.raw_value);
}

/**
 * @brief Multiply a Fixed_S16_3 by a Fixed_S32_16.
 *
 * @param a First factor.
 * @param b Second factor.
 * @return Product, as a Fixed_S16_3.
 */
static __inline__ Fixed_S16_3 Fixed_S16_3_S32_16_mul(Fixed_S16_3 a, Fixed_S32_16 b) {
  return Fixed_S16_3(a.raw_value * b.raw_value >> FIXED_S32_16_PRECISION);
}

/**
 * @brief Run a value through an Nth order linear recursive (IIR) filter.
 *
 * Computes y[n] = sum(cb[i] * x[n - i]) - sum(ca[i] * y[n - 1 - i]), a generalization of the
 * <a href="https://en.wikipedia.org/wiki/Digital_biquad_filter">digital biquad filter</a>.
 *
 * @param x Next input value, x[n].
 * @param num_input_coefficients Number of input taps, at least 1.
 * @param num_output_coefficients Number of output taps.
 * @param cb Input coefficients, @p num_input_coefficients entries.
 * @param ca Output coefficients, @p num_output_coefficients entries.
 * @param[in,out] state_x History of x, @p num_input_coefficients entries, kept between calls.
 * @param[in,out] state_y History of y, @p num_output_coefficients entries, kept between calls.
 * @return Filtered value, y[n].
 */
Fixed_S64_32 math_fixed_recursive_filter(Fixed_S64_32 x, int num_input_coefficients,
                                         int num_output_coefficients, const Fixed_S64_32 *cb,
                                         const Fixed_S64_32 *ca, Fixed_S64_32 *state_x,
                                         Fixed_S64_32 *state_y);

/** @} */
