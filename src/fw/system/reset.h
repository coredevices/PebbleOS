/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

//! Shut down system services but don't actually reset.
void system_reset_prepare(void);

//! Reset nicely after shutting down system services. Does not set the reboot_reason other than
//! calling reboot_reason_set_restarted_safely just before the reset occurs.
PBL_NORETURN void system_reset(void);

//! Same as system_reset() but usable as a callback.
void system_reset_callback(void *);

//! The final stage in the reset process.
PBL_NORETURN void system_hard_reset(void);
