/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup util_sort Sorting
 * @ingroup util
 * @brief In-place sorting of small arrays.
 * @{
 */

/** @brief qsort()-style comparator: negative, 0 or positive when a < b, a == b or a > b. */
typedef int (*SortComparator)(const void *, const void *);

/**
 * @brief Sort an array in ascending order with a quadratic exchange sort.
 *
 * Meant for small arrays; it is not stable.
 *
 * @param[in,out] array Array to sort.
 * @param num_elem Number of elements.
 * @param elem_size Size of an element in bytes.
 * @param comp Comparator.
 */
void sort_bubble(void *array, size_t num_elem, size_t elem_size, SortComparator comp);

/** @} */
