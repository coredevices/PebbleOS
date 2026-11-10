/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

struct pbl_thread;

//! Points arch_cur_user at @p t.
void arm_cur_user_set(struct pbl_thread *t);

//! The running thread is about to drop privilege.
void arm_cur_user_enter(void);

//! Handles the re-entry SVC that ends a pbl_user_call() callback,
//! redirecting the exception @p frame. @return false if it is not one.
bool arch_user_call_return(uint32_t *frame);

//! @return true if the SVC returning to @p caller_pc may raise privilege.
bool arch_syscall_raise_allowed(uint32_t caller_pc);

//! Records a syscall made from @p orig_sp and points its stacked return
//! address, at @p lr_slot, at the code that drops privilege again.
void arch_syscall_entered(uintptr_t orig_sp, uint32_t *lr_slot);
