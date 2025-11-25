/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void pwr_access_backup_domain(bool enable_access);

bool pwr_did_boot_from_standby(void);

void pwr_clear_boot_from_standby_flag(void);
