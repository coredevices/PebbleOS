/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

void prepare_for_software_failure(void);

//! Does not call reboot_reason_set.
[[noreturn]] void reset_due_to_software_failure(void);
