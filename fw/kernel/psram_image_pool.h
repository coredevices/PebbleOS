/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

//! Fixed-size slots in disposable external RAM, each holding one decoded image: its palette, then
//! its pixels. Allocation needs a PSRAM retention hold, and a slot's contents are only valid in the
//! retention epoch they were written in.

//! Size of a slot, enough for the largest image the imaging service accepts and a full palette.
#define PSRAM_IMAGE_SLOT_SIZE (44 * 1024)

#ifdef CONFIG_PSRAM_IMAGE_POOL
#define PSRAM_IMAGE_POOL_SLOTS CONFIG_PSRAM_IMAGE_SLOTS

//! Allocate a slot. A retention hold must be taken.
//! @return Slot, or NULL if all are in use.
void *psram_image_pool_alloc(void);

//! Free a slot.
void psram_image_pool_free(void *slot);

//! Get the number of slots in use.
unsigned int psram_image_pool_used(void);
#else
#define PSRAM_IMAGE_POOL_SLOTS 0

static inline void *psram_image_pool_alloc(void) {
  return NULL;
}

static inline void psram_image_pool_free(void *slot) {
}

static inline unsigned int psram_image_pool_used(void) {
  return 0;
}
#endif
