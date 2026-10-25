/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/heap.h>

void kernel_heap_init(void);

Heap *kernel_heap_get(void);

#ifdef CONFIG_KERNEL_HEAP_BOOT_MIN_HEADROOM
//! Smallest free space the kernel heap has had since boot, unlike
//! heap_get_minimum_headroom(), whose window the analytics heartbeat resets hourly.
uint32_t kernel_heap_get_boot_minimum_headroom(void);
#endif

#ifdef CONFIG_MALLOC_INSTRUMENTATION
void kernel_heap_dump_instrumentation(void);
#endif
