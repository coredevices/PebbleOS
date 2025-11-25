/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "resource/resource.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


ResAppNum sys_get_current_resource_num(void);

uint32_t sys_resource_load_file_as_resource(const char *filepath, const char *filename);

size_t sys_resource_size(ResAppNum app_num, uint32_t handle);

size_t sys_resource_load_range(ResAppNum app_num, uint32_t id, uint32_t start_bytes, uint8_t *buffer, size_t num_bytes);

const uint8_t * sys_resource_read_only_bytes(ResAppNum app_num, uint32_t resource_id,
                                             size_t *num_bytes_out);

uint32_t sys_resource_get_and_cache(ResAppNum app_num, uint32_t resource_id);

bool sys_resource_is_valid(ResAppNum app_num, uint32_t resource_id);

void fake_resource_syscalls_cleanup(void);
