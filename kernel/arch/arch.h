/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/debug.h>
#include <pbl/kernel/mpu.h>
#include <pbl/kernel/thread.h>

//! What the portable kernel needs from an architecture.

//! The running thread. The arch defines it so it can live where unprivileged
//! threads may read it: pbl_thread_current() runs without privilege.
extern struct pbl_thread *pbl_cur;

//! Sets pbl_cur, which may live in memory unprivileged threads cannot write.
void arch_cur_set(struct pbl_thread *t);

//! Early arch setup, before main(): fault exceptions.
void arch_init(void);

//! Builds the initial context so that the first switch to @p t runs
//! @p entry(@p arg) on the thread's stack.
void arch_thread_init(struct pbl_thread *t, void (*entry)(void *), void *arg);

//! Number of MPU slots switched in with each thread, the last ones of the MPU.
#define ARCH_THREAD_MPU_REGIONS 4

//! Leaves the MPU off with every slot empty, whatever came before the kernel.
void arch_mpu_init(void);

//! Turns memory protection on.
void arch_mpu_enable(void);

void arch_mpu_region_set(unsigned int slot, const struct pbl_mpu_region *region);

//! Copies into the P_RW_U_RO region in @p slot, negative if there is none yet.
void arch_mpu_user_ro_write(int slot, void *dst, const void *src, size_t n);

//! Reads back a slot. The name is left out, and so is everything but the access
//! and exec attributes, of which access is the closest the hardware can tell.
//! @return false if the slot is empty.
bool arch_mpu_region_get(unsigned int slot, struct pbl_mpu_region *region);

void arch_thread_regions_set(struct pbl_thread *t, const struct pbl_mpu_region *const *regions);

//! Starts running pbl_cur; never returns.
[[noreturn]] void arch_start(void);

//! Asks for a context switch. From a thread it happens once interrupts are
//! unlocked; from an ISR, when the ISR returns.
void arch_switch_request(void);

//! Ends the calling thread; never returns.
[[noreturn]] void arch_thread_exit(void);

//! Another thread has been aborted and will never be switched to again.
void arch_thread_aborted(struct pbl_thread *t);

bool arch_in_isr(void);

//! Rate of arch_cycle_get_32() from <pbl_arch_cycle.h>.
uint32_t arch_cycles_per_sec(void);

void arch_irq_disable(void);
void arch_irq_enable(void);

//! Lock of the kernel heap, usable before the kernel runs. Not nestable.
void arch_heap_lock(void);
void arch_heap_unlock(void);

void arch_thread_saved_regs(const struct pbl_thread *t, struct pbl_thread_saved_regs *regs);
void arch_thread_info_regs(const struct pbl_thread *t, uint32_t regs[PBL_THREAD_REG_COUNT]);

//! Idle-thread hook: nothing is runnable for up to @p max_ticks (0 when a
//! timeout is already due, PBL_TICK_FOREVER when none is pending).
void arch_idle(pbl_tick_t max_ticks);

//! Resets the core and its peripherals; called with interrupts locked.
[[noreturn]] void arch_reset(void);
