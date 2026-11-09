/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <errno.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/debug.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>

#include <arch.h>

//! Internal interface between the objects, the scheduler and the arch code.
//! Everything here is called with interrupts locked unless noted.

//! Woken by resume after being suspended while blocked.
#define KWAKE_INTERRUPTED (-EINTR)

void sched_init(void);
void sched_thread_start(struct pbl_thread *t);
void sched_thread_remove(struct pbl_thread *t);
void sched_thread_suspend(struct pbl_thread *t);
void sched_thread_resume(struct pbl_thread *t);
void sched_prio_set(struct pbl_thread *t, pbl_prio_t base, pbl_prio_t effective);
void sched_yield_current(void);
void sched_request_switch(void);

//! Blocks the current thread on @p wq (NULL for a plain sleep) until woken or
//! the timeout expires. Releases and re-takes the interrupt lock; the caller
//! must hold it exactly once. @return the wake code, -EAGAIN on timeout.
int sched_block(struct pbl_waitq *wq, pbl_timeout_t timeout);

//! Makes a blocked thread ready with @p rc as its wake code.
void sched_wake(struct pbl_thread *t, int rc);

void waitq_insert(struct pbl_waitq *wq, struct pbl_thread *t);
void waitq_remove(struct pbl_waitq *wq, struct pbl_thread *t);
struct pbl_thread *waitq_pop(struct pbl_waitq *wq);

//! Priority-inheritance helpers used by the mutex.
void sched_inherit(struct pbl_thread *owner, pbl_prio_t prio);
void sched_disinherit(struct pbl_thread *owner);

//! C entry from the reset vector: sets up RAM, then kernel_init().
[[noreturn]] void kernel_prep_c(void);

//! Brings up the SoC, interrupts, faults and the heap, then runs @p entry.
[[noreturn]] void kernel_init(int (*entry)(void));

//! Starts the scheduler: creates the idle thread and switches to the highest priority thread.
[[noreturn]] void pbl_kernel_start(void);

//! Resets the interrupt lock nesting when the first thread starts.
void irq_reset(void);

//! Creates the idle thread and picks the first thread to run.
void sched_start_prepare(void);

//! Arch entry points into the scheduler.
struct pbl_thread *sched_switch_in(void); // picks and accounts the next thread
//! Advances the tick count by @p ticks and expires the timeouts that fell due.
void sched_announce(uint64_t ticks);
bool sched_idle_confirm(void);
struct pbl_thread *sched_idle_thread(void);
//! Current tick count; call with the interrupt lock held.
pbl_tick_t sched_ticks(void);
//! Current tick count in 64 bits; call with the interrupt lock held.
uint64_t sched_uptime_ticks(void);

//! Starts the system timer, when the scheduler starts.
void timekeeping_start(void);
//! Catches the tick count up with the system clock source.
void timekeeping_announce(void);
//! Idles the CPU for up to @p ticks; call with every interrupt masked and idle confirmed.
void timekeeping_idle(pbl_tick_t ticks);
//! The uptime tick at which the wall clock reaches @p realtime_ms, rounded up. Interrupt lock held.
uint64_t timekeeping_realtime_to_ticks(uint64_t realtime_ms);
//! Realigns the wall clock work after the clock moved. Interrupt lock held.
void wallclock_clock_set(uint64_t old_ms, uint64_t new_ms);
void wallclock_reset_for_test(void);

//! Runs the callbacks of the expired timers, from the tick ISR.
void timer_expire(void);
//! Ticks from @p now until the next timer expires, PBL_TICK_FOREVER if none.
pbl_tick_t timer_ticks_until_next(uint64_t now);
void timer_reset_for_test(void);
//! Starts the threads of the system work queues.
void sys_workq_init(void);
void sys_workq_reset_for_test(void);
extern struct pbl_thread *pbl_all_threads;

//! What the MPU slots of a thread hold, after its PBL_THREAD_MAX_MEM_REGIONS.
#define THREAD_REGION_STACK_GUARD   PBL_THREAD_MAX_MEM_REGIONS
#define THREAD_REGION_SYSCALL_GUARD (PBL_THREAD_MAX_MEM_REGIONS + 1)
static_assert(THREAD_REGION_SYSCALL_GUARD + 1 == ARCH_THREAD_MPU_REGIONS,
              "every thread slot must have a use");

//! Takes over the MPU, adds the static regions it knows about and enables it.
void mpu_init(void);
