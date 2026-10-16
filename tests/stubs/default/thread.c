/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/debug.h>
#include <pbl/kernel/thread.h>

static uint32_t s_app_task_control_reg = 0;

PBL_WEAK int pbl_thread_create(struct pbl_thread *t, const struct pbl_thread_attr *attr) {
  return 0;
}

PBL_WEAK void pbl_thread_abort(struct pbl_thread *t) {
}

PBL_WEAK void pbl_thread_suspend(struct pbl_thread *t) {
}

PBL_WEAK void pbl_thread_resume(struct pbl_thread *t) {
}

PBL_WEAK void pbl_thread_yield(void) {
}

PBL_WEAK struct pbl_thread *pbl_thread_current(void) {
  return nullptr;
}

PBL_WEAK struct pbl_thread *pbl_thread_idle(void) {
  return nullptr;
}

PBL_WEAK void pbl_thread_prio_set(struct pbl_thread *t, pbl_prio_t prio) {
}

PBL_WEAK pbl_prio_t pbl_thread_prio_get(const struct pbl_thread *t) {
  return 0;
}

PBL_WEAK enum pbl_thread_state pbl_thread_state(const struct pbl_thread *t) {
  return PBL_THREAD_READY;
}

PBL_WEAK void pbl_thread_regions_set(struct pbl_thread *t, const MpuRegion *const *regions) {
}

PBL_WEAK void pbl_thread_region_set(struct pbl_thread *t, const MpuRegion *region) {
}

PBL_WEAK void pbl_thread_saved_regs(const struct pbl_thread *t,
                                    struct pbl_thread_saved_regs *regs) {
  *regs = (struct pbl_thread_saved_regs){.control = s_app_task_control_reg};
}

PBL_WEAK void pbl_thread_stack_info(const struct pbl_thread *t,
                                    struct pbl_thread_stack_info *info) {
  *info = (struct pbl_thread_stack_info){};
}

PBL_WEAK void stub_control_reg(uint32_t reg) {
  s_app_task_control_reg = reg;
}
