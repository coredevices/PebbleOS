/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/work.h>

PBL_WEAK void pbl_delayed_work_expired(struct pbl_timer *timer) {
}

PBL_WEAK void pbl_delayed_work_init(struct pbl_delayed_work *dwork, pbl_work_fn_t fn) {
  *dwork = (struct pbl_delayed_work)PBL_DELAYED_WORK_INITIALIZER(fn);
}

PBL_WEAK bool pbl_delayed_work_is_pending(const struct pbl_delayed_work *dwork) {
  return false;
}

PBL_WEAK bool pbl_delayed_work_schedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                                        pbl_timeout_t delay) {
  return true;
}

PBL_WEAK bool pbl_delayed_work_reschedule(struct pbl_workq *q, struct pbl_delayed_work *dwork,
                                          pbl_timeout_t delay) {
  return false;
}

PBL_WEAK void pbl_delayed_work_schedule_next(struct pbl_delayed_work *dwork, pbl_tick_t period) {
}

PBL_WEAK bool pbl_delayed_work_cancel(struct pbl_delayed_work *dwork) {
  return false;
}

PBL_WEAK bool pbl_delayed_work_cancel_sync(struct pbl_delayed_work *dwork) {
  return false;
}

PBL_WEAK pbl_tick_t pbl_delayed_work_remaining(const struct pbl_delayed_work *dwork) {
  return 0U;
}

PBL_WEAK bool pbl_work_is_running(const struct pbl_work *work) {
  return false;
}

PBL_WEAK struct pbl_workq pbl_sys_workq;
PBL_WEAK struct pbl_workq pbl_sys_workq_high;

PBL_WEAK void pbl_wallclock_work_init(struct pbl_wallclock_work *w, pbl_work_fn_t fn) {
  *w = (struct pbl_wallclock_work){};
  w->work.fn = fn;
}

PBL_WEAK void pbl_wallclock_work_start(struct pbl_workq *q, struct pbl_wallclock_work *w,
                                       uint32_t unit_s, uint32_t every) {
  w->running = true;
}

PBL_WEAK bool pbl_wallclock_work_stop(struct pbl_wallclock_work *w) {
  const bool was_running = w->running;

  w->running = false;
  return was_running;
}

PBL_WEAK bool pbl_wallclock_work_is_running(const struct pbl_wallclock_work *w) {
  return w->running;
}
