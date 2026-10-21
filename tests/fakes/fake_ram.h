/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <string.h>

#include <pbl/drivers/ram.h>
#include <pbl/kernel/scratch.h>

#include <clar.h>

// External RAM the size of the scratch heap, which is built on top of it
static uint8_t s_fake_ram_mem[CONFIG_SCRATCH_HEAP_SIZE] __attribute__((aligned(1024)));
static struct pbl_ram_state s_fake_ram_state;
static const struct pbl_ram s_fake_ram = {
  .state = &s_fake_ram_state,
};
static int s_fake_ram_get_ret;

const struct pbl_ram *const EXTRAM = &s_fake_ram;

void pbl_ram_get_region(const struct pbl_ram *ram, uintptr_t *base, size_t *size) {
  *base = (uintptr_t)s_fake_ram_mem;
  *size = sizeof(s_fake_ram_mem);
}

int pbl_ram_get(const struct pbl_ram *ram) {
  if (s_fake_ram_get_ret != 0) {
    return s_fake_ram_get_ret;
  }
  if (ram->state->users++ == 0U) {
    // It may have been off: nothing survives that
    memset(s_fake_ram_mem, 0xa5, sizeof(s_fake_ram_mem));
    ram->state->power_cycles++;
  }
  return 0;
}

void pbl_ram_put(const struct pbl_ram *ram) {
  cl_assert(ram->state->users > 0U);
  ram->state->users--;
}

//! Whether the memory is in use, and so kept powered.
static inline bool fake_ram_in_use(void) {
  return s_fake_ram_state.users > 0U;
}

//! Make powering the memory on fail with this error, 0 to succeed.
static inline void fake_ram_set_get_result(int ret) {
  s_fake_ram_get_ret = ret;
}

//! Live scratch heap allocations.
static inline unsigned int fake_ram_allocations(void) {
  struct pbl_scratch_stats stats;

  pbl_scratch_get_stats(&stats);
  return stats.allocations;
}
