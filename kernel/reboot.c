/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <limits.h>
#include <stdatomic.h>
#include <stddef.h>

#include <pbl/kernel/irq.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/sched.h>

#include <arch.h>

#ifndef PBL_NO_LINKER_SCRIPT
extern const struct pbl_sys_off_handler __pbl_sys_off_handlers_start[];
extern const struct pbl_sys_off_handler __pbl_sys_off_handlers_end[];
#else
extern const struct pbl_sys_off_handler __pbl_sys_off_handlers_start[] PBL_UNSORTED_SECTION_START(
    pbl_sys_off);
extern const struct pbl_sys_off_handler __pbl_sys_off_handlers_end[] PBL_UNSORTED_SECTION_END(
    pbl_sys_off);
#endif

static atomic_bool s_in_progress;

// Handlers are few, so they are ordered here rather than by the linker, which
// host builds don't use.
static void prv_run(enum pbl_sys_off_mode mode) {
  const struct pbl_sys_off_handler *last = nullptr;

  for (;;) {
    const struct pbl_sys_off_handler *next = nullptr;

    for (const struct pbl_sys_off_handler *h = __pbl_sys_off_handlers_start;
         h < __pbl_sys_off_handlers_end; h++) {
      if (h->mode != mode) {
        continue;
      }
      if (last != nullptr &&
          (h->priority > last->priority || (h->priority == last->priority && h <= last))) {
        continue;
      }
      if (next == nullptr || h->priority > next->priority) {
        next = h;
      }
    }

    if (next == nullptr) {
      return;
    }
    next->handler(mode, next->user_data);
    last = next;
  }
}

[[noreturn]] static void prv_reset(void) {
  pbl_irq_lock();
#ifdef CONFIG_SOC_HAS_RESET
  pbl_soc_reset();
#else
  arch_reset();
#endif
}

static void prv_prepare(enum pbl_sys_off_mode mode) {
  bool reentered = atomic_exchange(&s_in_progress, true);

  if (!reentered && !pbl_in_isr() && !pbl_irq_is_locked() && pbl_kernel_is_running()) {
    prv_run(mode);
  }
}

[[noreturn]] void pbl_reboot(void) {
  prv_prepare(PBL_SYS_OFF_RESTART_PREPARE);
  prv_run(PBL_SYS_OFF_RESTART);
  prv_reset();
}

[[noreturn]] void pbl_power_off(void) {
  prv_prepare(PBL_SYS_OFF_POWER_OFF_PREPARE);
  prv_run(PBL_SYS_OFF_POWER_OFF);
  prv_reset();
}

[[noreturn]] void pbl_reboot_emergency(void) {
  prv_reset();
}
