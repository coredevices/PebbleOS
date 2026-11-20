/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <limits.h>

#include <pbl/kernel/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_idle_hook, idle_hooks);
PBL_ITERABLE_SECTION_DECLARE(const struct pbl_idle_hook, idle_late);

// One bit per hook.
typedef uint32_t idle_hook_mask_t;
#define IDLE_HOOKS_MAX (sizeof(idle_hook_mask_t) * CHAR_BIT)

#ifndef CONFIG_SOC_HAS_IDLE_STATES
const struct pbl_idle_state pbl_soc_idle_states[] = {
  {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = pbl_cpu_idle},
};
const size_t pbl_soc_num_idle_states = PBL_ARRAY_SIZE(pbl_soc_idle_states);
#endif

static uint64_t s_residency_us[PBL_IDLE_STATES_MAX];
static uint8_t s_blocks[PBL_IDLE_STATES_MAX];
static struct pbl_idle_latency_request *s_requests;
// Longest time to wake up every request tolerates.
static pbl_tick_t s_latency_ticks = PBL_TICK_FOREVER;
static bool s_disabled;

#ifdef CONFIG_RAMFUNC
extern uint8_t __ramfunc_start[];
extern uint8_t __ramfunc_end[];

static bool prv_runs_from_ram(void *fn) {
  return fn == nullptr || ((uint8_t *)fn >= __ramfunc_start && (uint8_t *)fn < __ramfunc_end);
}
#endif

void idle_init(void) {
  PBL_ASSERTN((size_t)(PBL_ITERABLE_SECTION_END(idle_hooks) -
                       PBL_ITERABLE_SECTION_START(idle_hooks)) <= IDLE_HOOKS_MAX);

#ifdef CONFIG_RAMFUNC
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_idle_hook, idle_late, hook) {
    PBL_ASSERTN(prv_runs_from_ram(hook->enter) && prv_runs_from_ram(hook->exit));
  }
#endif
}

// The hooks worth calling for @p ticks that wake up in time, one bit each; adds the time they
// take to wake up.
static idle_hook_mask_t prv_hooks_select(pbl_tick_t ticks, pbl_tick_t *exit_ticks) {
  idle_hook_mask_t hooks = 0U;
  size_t n = 0U;

  for (const struct pbl_idle_hook *hook = PBL_ITERABLE_SECTION_START(idle_hooks);
       hook < PBL_ITERABLE_SECTION_END(idle_hooks); hook++, n++) {
    const struct pbl_idle_hook_times *times = hook->times;

    if (times->min_ticks != 0U && ticks >= times->min_ticks &&
        *exit_ticks + times->exit_ticks <= s_latency_ticks) {
      hooks |= PBL_BIT(n);
      *exit_ticks += times->exit_ticks;
    }
  }

  return hooks;
}

static void prv_hooks_enter(idle_hook_mask_t hooks, pbl_idle_state_id_t state) {
  size_t n = 0U;

  for (const struct pbl_idle_hook *hook = PBL_ITERABLE_SECTION_START(idle_hooks);
       hook < PBL_ITERABLE_SECTION_END(idle_hooks); hook++, n++) {
    if ((hooks & PBL_BIT(n)) != 0U && hook->enter != nullptr) {
      hook->enter(state, hook->arg);
    }
  }
}

static void prv_hooks_exit(idle_hook_mask_t hooks, pbl_idle_state_id_t state) {
  size_t n =
      (size_t)(PBL_ITERABLE_SECTION_END(idle_hooks) - PBL_ITERABLE_SECTION_START(idle_hooks));

  for (const struct pbl_idle_hook *hook = PBL_ITERABLE_SECTION_END(idle_hooks);
       hook > PBL_ITERABLE_SECTION_START(idle_hooks);) {
    hook--;
    n--;
    if ((hooks & PBL_BIT(n)) != 0U && hook->exit != nullptr) {
      hook->exit(state, hook->arg);
    }
  }
}

void idle_enter(pbl_tick_t ticks) {
  const struct pbl_idle_state *state;
  pbl_tick_t exit_ticks;
  idle_hook_mask_t hooks;
  uint64_t start_us;
  int i;

  if (s_disabled || ticks == 0U) {
    return;
  }

  PBL_ASSERTN(pbl_soc_num_idle_states <= PBL_IDLE_STATES_MAX);

  // The deepest state short of the shallowest blocked one that is worth entering and wakes up
  // in time.
  for (i = 0; i < (int)pbl_soc_num_idle_states && s_blocks[i] == 0U; i++) {
  }
  for (i--; i >= 0; i--) {
    if (ticks >= pbl_soc_idle_states[i].min_ticks &&
        pbl_soc_idle_states[i].exit_ticks <= s_latency_ticks) {
      break;
    }
  }
  if (i < 0) {
    return;
  }
  state = &pbl_soc_idle_states[i];

  exit_ticks = state->exit_ticks;
  hooks = prv_hooks_select(ticks, &exit_ticks);

  start_us = timekeeping_idle_begin(ticks, exit_ticks);
  prv_hooks_enter(hooks, (pbl_idle_state_id_t)i);
  // The interrupt lock would keep the kernel's interrupts from waking the CPU up.
  state->enter();
  prv_hooks_exit(hooks, (pbl_idle_state_id_t)i);
  s_residency_us[i] += timekeeping_idle_end() - start_us;
}

PBL_SECTION_RAM void pbl_idle_hooks_late_enter(pbl_idle_state_id_t state) {
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_idle_hook, idle_late, hook) {
    if (hook->enter != nullptr) {
      hook->enter(state, hook->arg);
    }
  }
}

PBL_SECTION_RAM void pbl_idle_hooks_late_exit(pbl_idle_state_id_t state) {
  for (const struct pbl_idle_hook *hook = PBL_ITERABLE_SECTION_END(idle_late);
       hook > PBL_ITERABLE_SECTION_START(idle_late);) {
    hook--;
    if (hook->exit != nullptr) {
      hook->exit(state, hook->arg);
    }
  }
}

void pbl_idle_hook_set_times(const struct pbl_idle_hook *hook, uint32_t min_us, uint32_t exit_us) {
  pbl_tick_t min_ticks = PBL_US_TO_TICKS_CEIL(min_us);

  unsigned int key = arch_irq_lock();
  hook->times->min_ticks = min_ticks != 0U ? min_ticks : 1U;
  hook->times->exit_ticks = PBL_US_TO_TICKS_CEIL(exit_us);
  arch_irq_unlock(key);
}

void pbl_idle_state_block(pbl_idle_state_id_t state) {
  PBL_ASSERTN(state < PBL_IDLE_STATES_MAX);

  unsigned int key = arch_irq_lock();
  PBL_ASSERTN(s_blocks[state] < UINT8_MAX);
  s_blocks[state]++;
  arch_irq_unlock(key);
}

void pbl_idle_state_release(pbl_idle_state_id_t state) {
  PBL_ASSERTN(state < PBL_IDLE_STATES_MAX);

  unsigned int key = arch_irq_lock();
  PBL_ASSERTN(s_blocks[state] > 0U);
  s_blocks[state]--;
  arch_irq_unlock(key);
}

static void prv_latency_update(void) {
  s_latency_ticks = PBL_TICK_FOREVER;
  for (const struct pbl_idle_latency_request *req = s_requests; req != nullptr; req = req->next) {
    s_latency_ticks = PBL_MIN(s_latency_ticks, req->ticks);
  }
}

void pbl_idle_latency_request_add(struct pbl_idle_latency_request *req, uint32_t us) {
  req->ticks = (pbl_tick_t)(((uint64_t)us * PBL_TICK_HZ) / 1000000U);

  unsigned int key = arch_irq_lock();
  req->next = s_requests;
  s_requests = req;
  prv_latency_update();
  arch_irq_unlock(key);
}

void pbl_idle_latency_request_remove(struct pbl_idle_latency_request *req) {
  unsigned int key = arch_irq_lock();
  for (struct pbl_idle_latency_request **it = &s_requests; *it != nullptr; it = &(*it)->next) {
    if (*it == req) {
      *it = req->next;
      break;
    }
  }
  prv_latency_update();
  arch_irq_unlock(key);
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
