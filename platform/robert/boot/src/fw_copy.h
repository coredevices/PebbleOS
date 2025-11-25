/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void fw_copy_check_update_fw(void);

//! @return false if we've failed to load recovery mode too many times and we should just sadwatch
bool fw_copy_switch_to_recovery_fw(void);
