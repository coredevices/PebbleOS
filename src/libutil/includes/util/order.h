/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

//! A Comparator returns the Order in which (a, b) occurs
//! @return negative int for a descending value (a > b), positive for an ascending value (b > a), 0 for equal
typedef int (*Comparator)(void *a, void *b);

int uint32_comparator(void *a, void *b);
