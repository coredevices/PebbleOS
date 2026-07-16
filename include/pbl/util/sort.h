/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stddef.h>

//! Standard sort comparator function
typedef int (*SortComparator)(const void *, const void *);

//! Bubble sorts an array
//! @param[in] array The array that should be sorted
//! @param[in] num_elem Number of elements in the array
//! @param[in] elem_size Size of each element in the array
//! @param[in] comp SortComparator comparator function
void sort_bubble(void *array, size_t num_elem, size_t elem_size, SortComparator comp);
