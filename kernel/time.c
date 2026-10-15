/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>

#include <arch.h>

#define BUSY_WAIT_CHUNK (1UL << 30)

// Whole ticks covering @p n units of @p hz, plus the partial tick in progress.
static pbl_tick_t prv_sleep_ticks(uint32_t n, uint32_t hz) {
  uint64_t ticks = ((uint64_t)n * PBL_TICK_HZ + hz - 1) / hz + 1;
  return ticks < PBL_TICK_FOREVER ? (pbl_tick_t)ticks : PBL_TICK_FOREVER - 1;
}

uint64_t pbl_uptime_ms(void) {
  return pbl_uptime_ticks() * 1000U / PBL_TICK_HZ;
}

uint32_t pbl_cycles_per_sec(void) {
  return arch_cycles_per_sec();
}

void pbl_busy_wait_us(uint32_t us) {
  // Cycles per microsecond, rounded up.
  uint32_t per_us = (arch_cycles_per_sec() + 999999U) / 1000000U;
  uint64_t cycles = (uint64_t)us * per_us;
  uint32_t start = arch_cycle_get_32();

  // Chunks short enough for 32-bit differences.
  while (cycles > BUSY_WAIT_CHUNK) {
    while (arch_cycle_get_32() - start < BUSY_WAIT_CHUNK) {
    }
    start += BUSY_WAIT_CHUNK;
    cycles -= BUSY_WAIT_CHUNK;
  }
  while (arch_cycle_get_32() - start < (uint32_t)cycles) {
  }
}

void pbl_sleep_ms(uint32_t ms) {
  if (ms == 0) {
    pbl_thread_yield();
    return;
  }
  pbl_thread_sleep(PBL_TICKS(prv_sleep_ticks(ms, 1000U)));
}

void pbl_sleep_us(uint32_t us) {
  if (us == 0) {
    pbl_thread_yield();
    return;
  }
  pbl_thread_sleep(PBL_TICKS(prv_sleep_ticks(us, 1000000U)));
}
