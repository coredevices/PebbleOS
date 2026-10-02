/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup util_size Sizes
 * @ingroup util
 * @brief Array and member size helpers.
 * @{
 */

/**
 * Calculate the length of an array, based on the size of the element type.
 * @param array The array to be evaluated.
 * @return The length of the array.
 */
#define ARRAY_LENGTH(array) (sizeof((array)) / sizeof((array)[0]))

/**
 * @brief Get the number of elements of an array literal, as a compile-time constant.
 *
 * @code{.c}
 * #define RATES {10, 25, 50}
 * static const uint16_t s_rates[] = RATES;
 * _Static_assert(STATIC_ARRAY_LENGTH(uint16_t, RATES) == 3, "");
 * @endcode
 *
 * @param type Type of the elements.
 * @param array Brace-enclosed array literal.
 * @return Number of elements.
 */
#define STATIC_ARRAY_LENGTH(type, array) (sizeof((type[])array) / sizeof(type))

/**
 * @brief Get the size of a structure member without an instance.
 *
 * @param type Structure type.
 * @param member Member name.
 * @return Size of the member in bytes.
 */
#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

/** @} */
