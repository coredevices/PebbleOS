/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/heap.h"

void kernel_heap_init(void);

Heap* kernel_heap_get(void);

