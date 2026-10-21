/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup kernel_psram Disposable external RAM
 * @ingroup kernel
 * @brief Retention holds for external RAM that is powered down when unused.
 *
 * Disposable external RAM (@ref PBL_SECTION_DISPOSABLE) only keeps its contents while a retention
 * hold is taken. With no hold, deep sleep powers it down and its contents are lost; the epoch then
 * changes. Holds never block deep sleep, they only make it retain the contents. External RAM in
 * @ref PBL_SECTION_EXTRAM is never lost: any data there keeps the external RAM retained.
 *
 * Data written with a hold taken stays valid for as long as the epoch it was written in, so a
 * cache can record psram_epoch() with each entry and treat a different epoch as a miss.
 *
 * @code{.c}
 * psram_retain_acquire();
 * if (entry->epoch != psram_epoch()) {
 *   prv_refill(entry);
 *   entry->epoch = psram_epoch();
 * }
 * prv_use(entry);
 * psram_retain_release();
 * @endcode
 * @{
 */

#ifdef CONFIG_PSRAM_RETENTION

/**
 * @brief Take a retention hold.
 *
 * Powers the external RAM back up if it was powered down. Refcounted; balance each call with
 * psram_retain_release(). Not callable from ISRs.
 */
void psram_retain_acquire(void);

/**
 * @brief Release a retention hold.
 *
 * Once the last hold is released, the next deep sleep may power the external RAM down.
 */
void psram_retain_release(void);

/**
 * @brief Check whether a retention hold is taken.
 *
 * @return true if at least one hold is taken.
 */
bool psram_is_retained(void);

/**
 * @brief Get the retention epoch.
 *
 * @return A value that changes every time the external RAM loses its contents.
 */
uint32_t psram_epoch(void);

#else

static inline void psram_retain_acquire(void) {
}

static inline void psram_retain_release(void) {
}

static inline bool psram_is_retained(void) {
  return true;
}

static inline uint32_t psram_epoch(void) {
  return 0;
}

#endif

/** @} */
