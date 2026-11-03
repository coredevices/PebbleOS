/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/time.h>
#include <pbl/kernel/types.h>

pbl_tick_t pbl_ms_to_ticks(uint32_t ms) {
  return ((uint64_t)ms * PBL_TICK_HZ) / 1000;
}

uint32_t pbl_ticks_to_ms(pbl_tick_t ticks) {
  return ((uint64_t)ticks * 1000) / PBL_TICK_HZ;
}
