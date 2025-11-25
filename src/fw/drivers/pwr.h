/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void pwr_enable_wakeup(bool enable);

void pwr_flash_power_down_stop_mode(bool power_down);

void pwr_access_backup_domain(bool enable_access);
