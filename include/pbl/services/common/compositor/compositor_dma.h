/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void compositor_dma_init(void);
void compositor_dma_run(void *to, const void *from, uint32_t size);
