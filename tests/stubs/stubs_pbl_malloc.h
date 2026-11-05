/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

void stub_pbl_malloc_set_should_fail(bool should_fail);
void stub_pbl_malloc_set_max_size(size_t max_size);
