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
//! the timeout expires. Releases the interrupt lock, taken with @p key, to
//! switch, and holds it again on return. @return the wake code, -EAGAIN on timeout.
int sched_block(struct pbl_waitq *wq, pbl_timeout_t timeout, unsigned int key);

//! Makes a blocked thread ready with @p rc as its wake code.
void sched_wake(struct pbl_thread *t, int rc);

void waitq_insert(struct pbl_waitq *wq, struct pbl_thread *t);
void waitq_remove(struct pbl_waitq *wq, struct pbl_thread *t);
struct pbl_thread *waitq_pop(struct pbl_waitq *wq);

//! Priority @p t runs at: its own, or that of the most urgent thread waiting for a mutex it holds.
pbl_prio_t mutex_inherited_prio(const struct pbl_thread *t);

//! C entry from the reset vector: sets up RAM, then kernel_init().
[[noreturn]] void kernel_prep_c(void);

//! Brings up the SoC, interrupts, faults and the heap, then runs @p entry.
[[noreturn]] void kernel_init(int (*entry)(void));

//! Starts the scheduler: creates the idle thread and switches to the highest priority thread.
[[noreturn]] void pbl_kernel_start(void);

//! Forgets the pbl_irq_lock() nesting of threads a test left behind.
void irq_reset_for_test(void);

//! Creates the idle thread and picks the first thread to run. Returns with the interrupt lock
//! held, which the arch drops as it starts that thread.
void sched_start_prepare(void);

//! The thread to run: the first of the most urgent ready ones. On a switch the arch makes it
//! pbl_cur, with the interrupt lock held from reading it to setting pbl_cur.
extern struct pbl_thread *sched_next;

//! Arch entry points into the scheduler.
//! Advances the tick count by @p ticks.
void sched_announce(uint64_t ticks);
//! Ends the time slice of the running thread; from the tick ISR, after timeout_expire().
void sched_slice(void);
//! The timeout of a blocked thread expired.
void sched_timed_out(struct pbl_thread *t);
bool sched_idle_confirm(void);
struct pbl_thread *sched_idle_thread(void);
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

//! The timeout queue, of blocked threads and timers alike. Interrupt lock held, but to expire.
//! @p to expires @p ticks from now.
void timeout_add(struct pbl_timeout *to, pbl_tick_t ticks);
//! Does nothing if @p to is not queued.
void timeout_remove(struct pbl_timeout *to);
//! Wakes the threads and runs the timers that fell due, from the tick ISR.
void timeout_expire(void);
//! Ticks until the next timeout, 0 if it is due and PBL_TICK_FOREVER if there is none.
pbl_tick_t timeout_ticks_until_next(void);
void timeout_reset_for_test(void);
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

//! MPU slot of the user read-only kernel data, -1 until it is added.
extern int mpu_user_ro_slot;
