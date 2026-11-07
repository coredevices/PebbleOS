/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/mpu.h>

#define KERNEL_READONLY_DATA PBL_SECTION(".kernel_unpriv_ro_bss")

//! Indices in pbl_thread_attr::regions.
#define MEMORY_LAYOUT_THREAD_REGION_APP           0
#define MEMORY_LAYOUT_THREAD_REGION_WORKER        1
#define MEMORY_LAYOUT_THREAD_REGION_SYSCALL_GUARD 3

void memory_layout_dump_mpu_regions(void);

void memory_layout_setup_mpu(void);

const struct pbl_mpu_region *memory_layout_get_app_region(void);

const struct pbl_mpu_region *memory_layout_get_readonly_bss_region(void);
const struct pbl_mpu_region *memory_layout_get_microflash_region(void);

const struct pbl_mpu_region *memory_layout_get_worker_region(void);

//! Gives every thread that does not set its own the kernel view of App and Worker RAM.
void memory_layout_thread_regions_init(void);

bool memory_layout_is_pointer_in_region(const struct pbl_mpu_region *region, const void *ptr);
bool memory_layout_is_buffer_in_region(const struct pbl_mpu_region *region, const void *buf,
                                       size_t length);
bool memory_layout_is_cstring_in_region(const struct pbl_mpu_region *region, const char *str,
                                        size_t max_length);
