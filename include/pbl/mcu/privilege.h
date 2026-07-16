/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

inline static bool mcu_state_is_thread_privileged(void);

//! Update the thread mode privilege bit in the control register. Note that you
//! must already be privileged to call this with a true argument.
void mcu_state_set_thread_privilege(bool privilege);

bool mcu_state_is_privileged(void);

//! Bracket a call to @p fn so its body runs in unprivileged thread mode while
//! the caller stays privileged.
//!
//! Must be called from privileged thread mode (asserts and behaves
//! unpredictably otherwise).
//!
//! Use this to invoke untrusted callbacks (e.g. JavaScript FFI dispatch) so
//! that any kernel-memory or peripheral access inside @p fn faults the MPU
//! instead of silently succeeding under the runtime's privileged context.
//!
//! The re-entry SVC is private to this helper. It is not part of the normal
//! syscall island, and is accepted only while this helper is active for the
//! current task.
void mcu_call_unprivileged(void (*fn)(void *), void *ctx);

#ifdef __arm__
#include "pbl/mcu/privilege_arm.inl.h"
#else
#include "pbl/mcu/privilege_stubs.inl.h"
#endif
