/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <kernel/pbl_malloc.h>

//! Kernel heap in disposable external RAM. Live allocations hold it retained; once it is empty,
//! its contents may be lost and are rebuilt by the next allocation. Without
//! CONFIG_KERNEL_VOLATILE_HEAP, these are the kernel heap functions.
#ifdef CONFIG_KERNEL_VOLATILE_HEAP
void *kernel_volatile_malloc(size_t size);
void *kernel_volatile_zalloc_check(size_t size);
void *kernel_volatile_realloc(void *ptr, size_t size);
void kernel_volatile_free(void *ptr);

//! Get the usage of the volatile heap, for diagnostics.
void kernel_volatile_heap_stats(unsigned int *used, unsigned int *high_water,
                                unsigned int *allocations);
#else
#define kernel_volatile_malloc       kernel_malloc
#define kernel_volatile_zalloc_check kernel_zalloc_check
#define kernel_volatile_realloc      kernel_realloc
#define kernel_volatile_free         kernel_free
#endif
