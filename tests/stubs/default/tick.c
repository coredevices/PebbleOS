/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/types.h>

PBL_WEAK pbl_tick_t pbl_ms_to_ticks(uint32_t ms) {
  const uint64_t ticks = ((uint64_t)ms * PBL_TICK_HZ) / 1000;

  return ticks < PBL_TICK_FOREVER ? (pbl_tick_t)ticks : PBL_TICK_FOREVER - 1U;
}

PBL_WEAK uint32_t pbl_ticks_to_ms(pbl_tick_t ticks) {
  return ((uint64_t)ticks * 1000) / PBL_TICK_HZ;
}
