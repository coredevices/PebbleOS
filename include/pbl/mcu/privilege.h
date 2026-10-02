/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup mcu_privilege Privilege
 * @ingroup mcu
 * @brief Thread-mode privilege control.
 *
 * Thread-mode privilege is CONTROL.nPRIV; exception handlers always run privileged. On host
 * builds (unit tests) code is always privileged.
 *
 * @code{.c}
 * static void prv_callback(void *ctx) {
 *   // runs unprivileged: kernel memory and peripherals fault here
 * }
 *
 * mcu_call_unprivileged(prv_callback, ctx);
 * @endcode
 * @{
 */

/**
 * @brief Check whether thread mode is privileged.
 *
 * Ignores whether an exception handler is running; see mcu_state_is_privileged().
 *
 * @return true if CONTROL.nPRIV is clear.
 */
inline static bool mcu_state_is_thread_privileged(void);

/**
 * @brief Set the thread-mode privilege bit in the CONTROL register.
 *
 * Dropping privilege is always possible; raising it requires already being privileged.
 *
 * @param privilege true for privileged, false for unprivileged.
 */
void mcu_state_set_thread_privilege(bool privilege);

/**
 * @brief Check whether the CPU currently runs privileged.
 *
 * @return true in privileged thread mode or in an exception handler.
 */
bool mcu_state_is_privileged(void);

/**
 * @brief Call a function in unprivileged thread mode while the caller stays privileged.
 *
 * Use it to invoke untrusted callbacks (e.g. JavaScript FFI dispatch) so that any kernel-memory
 * or peripheral access inside @p fn faults the MPU instead of silently succeeding under the
 * runtime's privileged context.
 *
 * Must be called from privileged thread mode (asserts and behaves unpredictably otherwise).
 * Privilege is restored on return through a re-entry SVC that is private to this helper: it is
 * not part of the normal syscall island, and is accepted only while this helper is active for
 * the current task.
 *
 * @param fn Function to call.
 * @param ctx Argument passed to @p fn.
 */
void mcu_call_unprivileged(void (*fn)(void *), void *ctx);

/** @} */

#ifdef __arm__
#include "pbl/mcu/privilege_arm.inl.h"
#else
#include "pbl/mcu/privilege_stubs.inl.h"
#endif
