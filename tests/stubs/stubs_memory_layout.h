/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/memory_layout.h"

const MpuRegion* memory_layout_get_app_region(void) { return NULL; }
const MpuRegion* memory_layout_get_app_stack_guard_region(void) { return NULL; }

const MpuRegion* memory_layout_get_readonly_bss_region(void) { return NULL; }
const MpuRegion* memory_layout_get_microflash_region(void) { return NULL; }

const MpuRegion* memory_layout_get_worker_region(void) { return NULL; }
const MpuRegion* memory_layout_get_worker_stack_guard_region(void) { return NULL; }

const MpuRegion* memory_layout_get_kernel_main_stack_guard_region(void) { return NULL; }
const MpuRegion* memory_layout_get_kernel_bg_stack_guard_region(void) { return NULL; }

bool memory_layout_is_pointer_in_region(const MpuRegion *region, const void *ptr) { return false; }
bool memory_layout_is_buffer_in_region(const MpuRegion *region, const void *buf,
                                       size_t length) { return false; }
bool memory_layout_is_cstring_in_region(const MpuRegion *region, const char *str,
                                        size_t max_length) { return false; }
