/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

/**
 * @defgroup util_order Ordering
 * @ingroup util
 * @brief Comparator type used by the sorted containers.
 * @{
 */

/**
 * @brief Compare two items.
 *
 * Note the sign convention, opposite to qsort().
 *
 * @param a First item.
 * @param b Second item.
 * @return Negative if @p a > @p b, positive if @p b > @p a, 0 if equal.
 */
typedef int (*Comparator)(void *a, void *b);

/**
 * @brief Comparator for @c uint32_t items.
 *
 * @param a Pointer to the first @c uint32_t.
 * @param b Pointer to the second @c uint32_t.
 * @return Negative if @p a > @p b, positive if @p b > @p a, 0 if equal.
 */
int uint32_comparator(void *a, void *b);

/** @} */
