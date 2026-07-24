/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/reboot_reason.h"
#include "pbl/util/attributes.h"

#if !UNITTEST
NORETURN
#else
void
#endif
sys_poweroff(void);
