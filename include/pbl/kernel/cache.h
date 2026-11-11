/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pbl_arch_cache.h>

/**
 * @defgroup kernel_cache Cache maintenance
 * @ingroup kernel
 * @brief Coherency of the CPU caches with memory other bus masters or the instruction fetch see.
 *
 * A flush writes dirty lines back to memory and keeps them cached; an invalidate discards lines so
 * the next access reads memory. Flush a buffer before a DMA engine reads it, and invalidate it
 * after a DMA engine wrote it. To run code that was written as data, flush the data cache and
 * invalidate the instruction cache over it.
 *
 * Range operations cover every line the range touches, and are inline so code running while the
 * flash is unreadable can use them. Invalidating discards writes to whatever else shares those
 * lines, so buffers that get invalidated are aligned to @ref PBL_DCACHE_LINE_SIZE and sized with
 * PBL_DCACHE_ROUND_UP():
 *
 * @code{.c}
 * static uint8_t rx_buf[PBL_DCACHE_ROUND_UP(RX_LEN)] PBL_ALIGNED(PBL_DCACHE_LINE_SIZE);
 *
 * pbl_dcache_invalidate_range(rx_buf, sizeof(rx_buf)); // after the DMA transfer
 * @endcode
 *
 * Without the cache an operation targets, it does nothing. The whole-cache operations exist only
 * on CPUs that select CPU_HAS_ICACHE or CPU_HAS_DCACHE.
 * @{
 */

/** @brief Data cache line size in bytes, 1 without a data cache. */
#define PBL_DCACHE_LINE_SIZE ARCH_DCACHE_LINE_SIZE

/** @brief Instruction cache line size in bytes, 1 without an instruction cache. */
#define PBL_ICACHE_LINE_SIZE ARCH_ICACHE_LINE_SIZE

/** @brief @p size rounded up to whole data cache lines. */
#define PBL_DCACHE_ROUND_UP(size) \
  (((size) + PBL_DCACHE_LINE_SIZE - 1U) & ~(PBL_DCACHE_LINE_SIZE - 1U))

/**
 * @brief Flush the data cache lines covering a range.
 *
 * @param addr Start of the range.
 * @param size Size of the range in bytes.
 */
static PBL_ALWAYS_INLINE void pbl_dcache_flush_range(const void *addr, size_t size) {
  arch_dcache_flush_range((uintptr_t)addr, size);
}

/**
 * @brief Invalidate the data cache lines covering a range.
 *
 * @param addr Start of the range.
 * @param size Size of the range in bytes.
 */
static PBL_ALWAYS_INLINE void pbl_dcache_invalidate_range(void *addr, size_t size) {
  arch_dcache_invalidate_range((uintptr_t)addr, size);
}

/**
 * @brief Flush, then invalidate the data cache lines covering a range.
 *
 * @param addr Start of the range.
 * @param size Size of the range in bytes.
 */
static PBL_ALWAYS_INLINE void pbl_dcache_flush_invalidate_range(void *addr, size_t size) {
  arch_dcache_flush_invalidate_range((uintptr_t)addr, size);
}

/**
 * @brief Invalidate the instruction cache lines covering a range.
 *
 * @param addr Start of the range.
 * @param size Size of the range in bytes.
 */
static PBL_ALWAYS_INLINE void pbl_icache_invalidate_range(const void *addr, size_t size) {
  arch_icache_invalidate_range((uintptr_t)addr, size);
}

#if defined(CONFIG_CPU_HAS_DCACHE) || defined(__DOXYGEN__)
/** @brief Invalidate, then enable the data cache. */
void pbl_dcache_enable(void);

/** @brief Flush and invalidate, then disable the data cache. */
void pbl_dcache_disable(void);

/** @brief Flush the whole data cache. */
void pbl_dcache_flush_all(void);

/** @brief Invalidate the whole data cache. */
void pbl_dcache_invalidate_all(void);

/** @brief Flush, then invalidate the whole data cache. */
void pbl_dcache_flush_invalidate_all(void);
#endif

#if defined(CONFIG_CPU_HAS_ICACHE) || defined(__DOXYGEN__)
/** @brief Invalidate, then enable the instruction cache. */
void pbl_icache_enable(void);

/** @brief Disable, then invalidate the instruction cache. */
void pbl_icache_disable(void);

/** @brief Invalidate the whole instruction cache. */
void pbl_icache_invalidate_all(void);
#endif

/** @} */
