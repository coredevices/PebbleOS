/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

void prepare_for_software_failure(void);

//! Does not call reboot_reason_set, only calls reboot_reason_set_restarted_safely if we were
//! able shut everything down nicely before rebooting.
NORETURN reset_due_to_software_failure(void);

