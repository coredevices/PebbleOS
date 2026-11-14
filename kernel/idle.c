/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <pbl/kernel/idle.h>
#include <pbl/util/math.h>

static uint64_t s_residency_us[PBL_IDLE_STATES_MAX];
static bool s_disabled;

void idle_enter(pbl_tick_t ticks) {
  const struct pbl_idle_state *state;
  uint64_t start_us;
  int i;

  if (s_disabled || ticks == 0U) {
    return;
  }

  PBL_ASSERTN(pbl_soc_num_idle_states <= PBL_IDLE_STATES_MAX);

  for (i = MIN(pbl_soc_idle_state_limit(), (int)pbl_soc_num_idle_states - 1); i >= 0; i--) {
    if (ticks >= pbl_soc_idle_states[i].min_ticks) {
      break;
    }
  }
  if (i < 0) {
    return;
  }
  state = &pbl_soc_idle_states[i];

  start_us = timekeeping_idle_begin(ticks, state->exit_ticks);
  // The interrupt lock would keep the kernel's interrupts from waking the CPU up.
  state->enter();
  s_residency_us[i] += timekeeping_idle_end() - start_us;
}

void pbl_idle_set_enabled(bool enable) {
  s_disabled = !enable;
}

uint64_t pbl_idle_residency_us(pbl_idle_state_id_t state) {
  uint64_t us;

  if (state >= PBL_IDLE_STATES_MAX) {
    return 0U;
  }

  unsigned int key = arch_irq_lock();
  us = s_residency_us[state];
  arch_irq_unlock(key);

  return us;
}
