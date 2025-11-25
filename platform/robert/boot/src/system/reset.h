/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Reset nicely after shutting down system services. Does not set the reboot_reason other than
//! calling reboot_reason_set_restarted_safely just before the reset occurs.
void system_reset(void)__attribute__((noreturn));

//! The final stage in the reset process.
void system_hard_reset(void) __attribute__((noreturn));
