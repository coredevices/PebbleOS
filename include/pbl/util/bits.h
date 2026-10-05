/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup util_bits Bits and bit fields
 * @ingroup util
 * @brief Bit, mask and register field macros.
 *
 * A field is described by a contiguous mask, usually built with PBL_GENMASK(). PBL_FIELD_PREP()
 * shifts a value into the field and PBL_FIELD_GET() extracts it, so the field position never has
 * to be spelled out. With a constant mask both fold to a plain shift and mask.
 *
 * @code{.c}
 * #define CTRL1_ODR_MASK  PBL_GENMASK(7, 4)
 * #define CTRL1_ODR_50HZ  0x4U
 * #define CTRL1_LP_EN     PBL_BIT(0)
 *
 * reg = PBL_FIELD_PREP(CTRL1_ODR_MASK, CTRL1_ODR_50HZ) | CTRL1_LP_EN;
 * odr = PBL_FIELD_GET(CTRL1_ODR_MASK, reg);
 * @endcode
 * @{
 */

/**
 * @brief Unsigned 32-bit value with a single bit set.
 *
 * @param n Bit position, 0 to 31.
 */
#define PBL_BIT(n) (1U << (n))

/**
 * @brief Unsigned 64-bit value with a single bit set.
 *
 * @param n Bit position, 0 to 63.
 */
#define PBL_BIT64(n) (1ULL << (n))

/**
 * @brief Unsigned 32-bit mask with the @p n least significant bits set.
 *
 * @param n Number of bits, 0 to 31.
 */
#define PBL_BIT_MASK(n) (PBL_BIT(n) - 1U)

/**
 * @brief Unsigned 64-bit mask with the @p n least significant bits set.
 *
 * @param n Number of bits, 0 to 63.
 */
#define PBL_BIT64_MASK(n) (PBL_BIT64(n) - 1ULL)

/**
 * @brief Unsigned 32-bit mask with bits @p l to @p h set, both included.
 *
 * @param h Most significant bit of the mask, 0 to 31.
 * @param l Least significant bit of the mask, at most @p h.
 */
#define PBL_GENMASK(h, l) (((2U << ((h) - (l))) - 1U) << (l))

/**
 * @brief Unsigned 64-bit mask with bits @p l to @p h set, both included.
 *
 * @param h Most significant bit of the mask, 0 to 63.
 * @param l Least significant bit of the mask, at most @p h.
 */
#define PBL_GENMASK64(h, l) (((2ULL << ((h) - (l))) - 1ULL) << (l))

/**
 * @brief Isolate the least significant set bit of a value.
 *
 * @param value Unsigned value, evaluated twice.
 * @return @p value with every bit cleared but its lowest set one, or 0 if @p value is 0.
 */
#define PBL_LSB_GET(value) ((value) & -(value))

/**
 * @brief Extract a field from a register value.
 *
 * @param mask Contiguous, non-zero field mask, evaluated twice.
 * @param value Register value.
 * @return Field value, shifted down to bit 0.
 */
#define PBL_FIELD_GET(mask, value) (((value) & (mask)) / PBL_LSB_GET(mask))

/**
 * @brief Prepare a field value for a register.
 *
 * @param mask Contiguous, non-zero field mask, evaluated twice.
 * @param value Field value; bits that do not fit the field are dropped.
 * @return @p value shifted into the field position and masked.
 */
#define PBL_FIELD_PREP(mask, value) (((value) * PBL_LSB_GET(mask)) & (mask))

/**
 * @brief Set or clear a bit in a variable.
 *
 * The mask is built in the type of @p var, so other bits are preserved whatever its width.
 *
 * @param var Unsigned integer variable to update, evaluated more than once.
 * @param bit Bit position, less than the width of @p var.
 * @param set Set the bit if true, clear it otherwise.
 */
#define PBL_WRITE_BIT(var, bit, set) \
  ((var) =                           \
       (set) ? ((var) | ((__typeof__(var))1 << (bit))) : ((var) & ~((__typeof__(var))1 << (bit))))

/** @} */
