/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Stand-in for the firmware header, with what ctl/zbt_ctl.c uses.
#pragma once
#include <stdbool.h>
#include <stdint.h>
void worker_manager_lend_ram(void);
bool worker_manager_ram_is_free(void);
void worker_manager_return_ram(void);
uint32_t worker_manager_launches_refused_ram_lent(void);
