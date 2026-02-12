/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/mcu_reboot_reason.h"
#include <stdint.h>

void debug_init(McuRebootReason reason);

void debug_print_last_launched_app(void);

