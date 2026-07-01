/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Instruction cache and data cache are entirely separate. Therefore, you must both flush data
// cache _and_ invalidate instruction cache for that region in order to properly execute new code.

// A cache flush means the data is written out from the cache into memory. A cache invalidate
// means the data in the cache is thrown out and will be reloaded from memory on the next access.
// A flush does keep the data still in cache, so if you want to write out and invalidate, you want
// to use flush_invalidate.

// All cache operations MUST operate on the cache line size. You can safely flush memory that isn't
// part of your buffer, but invalidation CAN AND WILL destroy other memory! Be very careful!

// The cache line size on Cortex-M7 is 32 bytes.

//! Enable instruction cache.
void icache_enable(void);
//! Disable instruction cache.
void icache_disable(void);
//! Returns whether or not ICache is enabled
bool icache_is_enabled(void);
//! Returns line size of ICache.
uint32_t icache_line_size(void);

//! Invalidate entire instruction cache.
void icache_invalidate_all(void);
//! Invalidate instruction cache for `addr` for `size` bytes.
//! `addr` and `size` should both be aligned by the cache line size.
void icache_invalidate(void *addr, size_t size);

//! Enable data cache.
void dcache_enable(void);
//! Disable data cache.
void dcache_disable(void);
//! Returns whether or not DCache is enabled
bool dcache_is_enabled(void);
//! Returns line size of DCache.
uint32_t dcache_line_size(void);

//! Flush entire data cache.
void dcache_flush_all(void);
//! Invalidate entire data cache.
void dcache_invalidate_all(void);
//! Flush, then invalidate entire data cache.
void dcache_flush_invalidate_all(void);

//! Flush data cache for `addr` for `size` bytes.
//! `addr` and `size` should both be aligned by the cache line size.
void dcache_flush(const void *addr, size_t size);
//! Invalidate data cache for `addr` for `size` bytes.
//! `addr` and `size` should both be aligned by the cache line size.
void dcache_invalidate(void *addr, size_t size);
//! Flush, then invalidate data cache for `addr` for `size` bytes.
//! `addr` and `size` should both be aligned by the cache line size.
void dcache_flush_invalidate(const void *addr, size_t size);

//! Aligns an address and size so that they are both aligned to the ICache line size, and still
//! covers the range requested.
void icache_align(uintptr_t *addr, size_t *size);
//! Aligns an address and size so that they are both aligned to the DCache line size, and still
//! covers the range requested.
void dcache_align(uintptr_t *addr, size_t *size);

//! For aligning things to work with the data cache, you will want to check what the cache line
//! size is, and align accordingly. However, the hardware peripheral also might require a minimum
//! alignment. So you pass the minimum alignment in bytes into `min`, and the return value is the
//! mask you can apply to get the minimum aligned address.
uint32_t dcache_alignment_mask_minimum(uint32_t min);
