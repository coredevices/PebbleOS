/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <resource/resource.h>

bool applib_resource_track_mmapped(const void *bytes);

bool applib_resource_is_mmapped(const void *bytes);

bool applib_resource_munmap(const void *bytes);

bool applib_resource_munmap_all();

void applib_resource_munmap_or_free(void *bytes);

void *applib_resource_mmap_or_load(ResAppNum app_num, uint32_t resource_id, size_t offset,
                                   size_t num_bytes, bool used_aligned);
