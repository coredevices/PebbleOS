/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

void *os_malloc(size_t size);
void *os_malloc_check(size_t size);
void os_free(void *ptr);
