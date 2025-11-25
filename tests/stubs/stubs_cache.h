/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "mcu/cache.h"

void icache_enable(void) {}
void icache_disable(void) {}
void icache_invalidate_all(void) {}

void dcache_enable(void) {}
void dcache_disable(void) {}

void dcache_flush_all(void) {}
void dcache_invalidate_all(void) {}
void dcache_flush_invalidate_all(void) {}

void dcache_flush(const void *addr, size_t size) {}
void dcache_invalidate(void *addr, size_t size) {}
void dcache_flush_invalidate(const void *addr, size_t size) {}

uint32_t dcache_alignment_mask_minimum(uint32_t min) { return min - 1; }
