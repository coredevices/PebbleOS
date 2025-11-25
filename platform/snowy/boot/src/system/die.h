/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Does not call reboot_reason_set, only calls reboot_reason_set_restarted_safely if we were
//! able shut everything down nicely before rebooting.
void reset_due_to_software_failure(void) __attribute__((noreturn));
