/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Handles the re-entry SVC that ends a pbl_user_call() callback,
//! redirecting the exception @p frame. @return false if it is not one.
bool arch_user_call_return(uint32_t *frame);

//! @return true if the SVC returning to @p caller_pc may raise privilege.
bool arch_syscall_raise_allowed(uint32_t caller_pc);

//! Records a syscall made from @p orig_sp and points its stacked return
//! address, at @p lr_slot, at the code that drops privilege again.
void arch_syscall_entered(uintptr_t orig_sp, uint32_t *lr_slot);
