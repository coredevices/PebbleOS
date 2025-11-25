/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void watchdog_init(void);
void watchdog_start(void);

bool watchdog_check_reset_flag(void);
