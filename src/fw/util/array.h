/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>

#define ARRAY_INDEX_IS_FIRST_INDEX(index) ((index) == 0)
#define ARRAY_INDEX_IS_LAST_INDEX(index, array_length) ((index) == ((array_length) - 1))

void array_remove_nulls(void **array, size_t *num);
