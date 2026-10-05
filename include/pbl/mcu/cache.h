/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup mcu_cache Cache
 * @ingroup mcu
 * @brief Instruction and data cache maintenance.
 *
 * The instruction and data caches are separate: to execute newly written code, flush the data
 * cache @e and invalidate the instruction cache for that region. A flush writes cached data back
 * to memory and keeps it cached; an invalidate discards cached data so it is reloaded from
 * memory on the next access; flush_invalidate does both.
 *
 * Range operations work on whole cache lines. Flushing memory beyond a buffer is harmless, but
 * invalidating it destroys any pending writes to the neighbouring data, so align buffers that
 * get invalidated (e.g. DMA receive buffers) with dcache_align() or
 * dcache_alignment_mask_minimum().
 *
 * On cores without a cache every operation is a no-op and the line size reads as 1.
 *
 * @code{.c}
 * uintptr_t addr = (uintptr_t)rx_buf;
 * size_t size = rx_len;
 *
 * dcache_align(&addr, &size);
 * dcache_invalidate((void *)addr, size); // after the DMA transfer, before reading rx_buf
 * @endcode
 * @{
 */

/**
 * @brief Largest data cache line of the supported cores, in bytes.
 *
 * A buffer aligned to it and sized in multiples of it owns every cache line it touches, so it can
 * be invalidated without harming neighbouring data; see DCACHE_ROUND_UP().
 */
#define DCACHE_LINE_SIZE_MAX 32U

/** @brief @p size rounded up to whole @ref DCACHE_LINE_SIZE_MAX lines. */
#define DCACHE_ROUND_UP(size) (((size) + DCACHE_LINE_SIZE_MAX - 1U) & ~(DCACHE_LINE_SIZE_MAX - 1U))

/** @brief Enable the instruction cache, invalidating it first. */
void icache_enable(void);
/** @brief Disable the instruction cache, invalidating it afterwards. */
void icache_disable(void);
/**
 * @brief Check whether the instruction cache is enabled.
 *
 * @return true if enabled.
 */
bool icache_is_enabled(void);
/**
 * @brief Get the instruction cache line size.
 *
 * Only valid once icache_enable() has run.
 *
 * @return Line size in bytes, 1 without an instruction cache.
 */
uint32_t icache_line_size(void);

/** @brief Invalidate the entire instruction cache. */
void icache_invalidate_all(void);
/**
 * @brief Invalidate the instruction cache for a range.
 *
 * @param addr Start address, aligned to the line size.
 * @param size Size in bytes, a multiple of the line size.
 */
void icache_invalidate(void *addr, size_t size);

/** @brief Enable the data cache, invalidating it first. */
void dcache_enable(void);
/** @brief Disable the data cache, flushing and invalidating it first. */
void dcache_disable(void);
/**
 * @brief Check whether the data cache is enabled.
 *
 * @return true if enabled.
 */
bool dcache_is_enabled(void);
/**
 * @brief Get the data cache line size.
 *
 * Only valid once dcache_enable() has run.
 *
 * @return Line size in bytes, 1 without a data cache.
 */
uint32_t dcache_line_size(void);

/** @brief Flush the entire data cache. */
void dcache_flush_all(void);
/** @brief Invalidate the entire data cache. */
void dcache_invalidate_all(void);
/** @brief Flush, then invalidate the entire data cache. */
void dcache_flush_invalidate_all(void);

/**
 * @brief Flush the data cache for a range.
 *
 * @param addr Start address, aligned to the line size.
 * @param size Size in bytes, a multiple of the line size.
 */
void dcache_flush(const void *addr, size_t size);
/**
 * @brief Invalidate the data cache for a range.
 *
 * @param addr Start address, aligned to the line size.
 * @param size Size in bytes, a multiple of the line size.
 */
void dcache_invalidate(void *addr, size_t size);
/**
 * @brief Flush, then invalidate the data cache for a range.
 *
 * @param addr Start address, aligned to the line size.
 * @param size Size in bytes, a multiple of the line size.
 */
void dcache_flush_invalidate(const void *addr, size_t size);

/**
 * @brief Widen a range to whole instruction cache lines.
 *
 * @param[in,out] addr Start address, rounded down to a line boundary.
 * @param[in,out] size Size in bytes, grown so the range still covers the original one.
 */
void icache_align(uintptr_t *addr, size_t *size);
/**
 * @brief Widen a range to whole data cache lines.
 *
 * @param[in,out] addr Start address, rounded down to a line boundary.
 * @param[in,out] size Size in bytes, grown so the range still covers the original one.
 */
void dcache_align(uintptr_t *addr, size_t *size);

/**
 * @brief Get the alignment mask for a buffer shared with a peripheral.
 *
 * Combines the data cache line size with an alignment the peripheral itself requires.
 *
 * @param min Minimum alignment in bytes, a power of two.
 * @return The larger of @p min and the line size, minus 1: the address bits that must be clear.
 */
uint32_t dcache_alignment_mask_minimum(uint32_t min);

/** @} */
