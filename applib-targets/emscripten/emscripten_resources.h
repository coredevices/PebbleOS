/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "resource/resource.h"

// transformed to int to avoid surpises between C->JS
typedef int (*ResourceReadCb)(int offset, uint8_t *buf, int num_bytes);
typedef int (*ResourceGetSizeCb)(void);

bool emx_resources_init(void);
void emx_resources_deinit(void);
size_t emx_resources_get_size(ResAppNum app_num, uint32_t resource_id);
size_t emx_resources_read(ResAppNum app_num,
                          uint32_t resource_id,
                          uint32_t offset,
                          uint8_t *buf,
                          size_t num_bytes);
uint32_t emx_resources_register_custom(ResourceReadCb read_cb, ResourceGetSizeCb get_size_cb);
void emx_resources_remove_custom(uint32_t resource_id);
