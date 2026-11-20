/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_size Sizes
 * @ingroup util
 * @brief Array and member size helpers.
 * @{
 */

/**
 * @brief Fail the build if the argument is not an array.
 *
 * A pointer, such as an array parameter, has the type of its first element's address; an array
 * does not.
 *
 * @param array Expression to check.
 * @return 0, as a constant expression.
 */
#define PBL_IS_ARRAY(array)                                                           \
  ((int)sizeof(struct {                                                               \
    _Static_assert(!PBL_TYPES_COMPATIBLE(__typeof__(array), __typeof__(&(array)[0])), \
                   "argument must be an array");                                      \
  }))

/**
 * @brief Get the number of elements of an array.
 *
 * Fails the build if @p array is a pointer.
 *
 * @param array Array.
 * @return Number of elements, as a constant expression.
 */
#define PBL_ARRAY_SIZE(array) ((size_t)(PBL_IS_ARRAY(array) + sizeof(array) / sizeof((array)[0])))

/**
 * @brief Get the number of elements of an array literal, as a compile-time constant.
 *
 * @code{.c}
 * #define RATES {10, 25, 50}
 * static const uint16_t s_rates[] = RATES;
 * static_assert(PBL_STATIC_ARRAY_SIZE(uint16_t, RATES) == 3, "");
 * @endcode
 *
 * @param type Type of the elements.
 * @param array Brace-enclosed array literal.
 * @return Number of elements.
 */
#define PBL_STATIC_ARRAY_SIZE(type, array) (sizeof((type[])array) / sizeof(type))

/**
 * @brief Get the size of a structure member without an instance.
 *
 * @param type Structure type.
 * @param member Member name.
 * @return Size of the member in bytes.
 */
#define PBL_SIZEOF_FIELD(type, member) sizeof(((type *)0)->member)

/** @} */
