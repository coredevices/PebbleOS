/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include "drivers/mcu_reboot_reason.h"

void watchdog_init(void);
void watchdog_start(void);
void watchdog_stop(void);

void watchdog_feed(void);

bool watchdog_check_reset_flag(void);
McuRebootReason watchdog_clear_reset_flag(void);
