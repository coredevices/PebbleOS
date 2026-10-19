/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup kernel_syscall Syscalls
 * @ingroup kernel
 * @brief Calls from unprivileged threads into privileged code.
 *
 * A syscall stub raises privilege, runs its body and drops privilege again on return. Only stubs
 * linked between @c __syscall_text_start__ and @c __syscall_text_end__ may raise privilege. A
 * thread created with a syscall stack runs the bodies there instead of on its own stack.
 * @{
 */

#ifdef CONFIG_USERSPACE
#include <pbl_arch_syscall.h>
#endif

/**
 * @brief Called on the way out of every syscall made from unprivileged code.
 *
 * Implemented by the application. Runs privileged, on the syscall's stack, just before privilege
 * is dropped; it may block, e.g. to hold a thread that is being killed.
 */
void pbl_kernel_syscall_exit(void);

/** @} */
