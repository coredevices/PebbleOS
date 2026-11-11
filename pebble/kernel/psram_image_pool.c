/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "psram_image_pool.h"

#ifdef CONFIG_PSRAM_IMAGE_POOL

#include <stdint.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/psram.h>
#include <pbl/kernel/section.h>
#include <pbl/util/testing.h>

static_assert(PSRAM_IMAGE_POOL_SLOTS <= 64, "the free map is 64 bits wide");

PBL_T_STATIC uint8_t
    s_slots[PSRAM_IMAGE_POOL_SLOTS][PSRAM_IMAGE_SLOT_SIZE] PBL_SECTION_DISPOSABLE PBL_ALIGNED(32);

// In SRAM, so it survives the loss of the slots
static uint64_t s_used;
static PBL_MUTEX_DEFINE(s_lock);

void *psram_image_pool_alloc(void) {
  void *slot = NULL;

  PBL_ASSERTN(psram_is_retained());

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (~s_used &
      ((PSRAM_IMAGE_POOL_SLOTS == 64) ? UINT64_MAX : ((1ULL << PSRAM_IMAGE_POOL_SLOTS) - 1U))) {
    const unsigned int i = __builtin_ctzll(~s_used);
    s_used |= 1ULL << i;
    slot = s_slots[i];
  }
  pbl_mutex_unlock(&s_lock);

  return slot;
}

void psram_image_pool_free(void *slot) {
  const uintptr_t offset = (uintptr_t)slot - (uintptr_t)s_slots;
  const unsigned int i = offset / PSRAM_IMAGE_SLOT_SIZE;

  PBL_ASSERTN((offset % PSRAM_IMAGE_SLOT_SIZE == 0U) && (i < PSRAM_IMAGE_POOL_SLOTS));

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN(s_used & (1ULL << i));
  s_used &= ~(1ULL << i);
  pbl_mutex_unlock(&s_lock);
}

unsigned int psram_image_pool_used(void) {
  return __builtin_popcountll(s_used);
}

#endif
