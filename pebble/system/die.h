/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

void prepare_for_software_failure(void);

//! Does not call reboot_reason_set.
[[noreturn]] void reset_due_to_software_failure(void);

//! Saves a core dump, if the system has them, and resets.
//! @param is_forced The user asked for it: replace a stored dump that has not been read yet, and
//! record this as the reboot reason.
[[noreturn]] void reset_with_core_dump(bool is_forced);
