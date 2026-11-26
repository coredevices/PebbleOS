/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_RRAMC_TASKS_COMMITWRITEBUF_OFFSET    0x0008
#define NRF54L_RRAMC_READY_OFFSET                   0x0400
#define NRF54L_RRAMC_BUFSTATUS_WRITEBUFEMPTY_OFFSET 0x0418
#define NRF54L_RRAMC_CONFIG_OFFSET                  0x0500
#define NRF54L_RRAMC_POWER_LOWPOWERCONFIG_OFFSET    0x0518
// Glitch detector, sharing the registers of the RRAMC
#define NRF54L_GLITCHDET_CONFIG_OFFSET 0x05a0

/* Register bit definitions */

#define RRAMC_READY_READY PBL_BIT(0)

#define RRAMC_BUFSTATUS_WRITEBUFEMPTY_EMPTY PBL_BIT(0)

#define RRAMC_CONFIG_WEN               PBL_BIT(0)
#define RRAMC_CONFIG_WRITEBUFSIZE_MASK PBL_GENMASK(13, 8)

#define RRAMC_POWER_LOWPOWERCONFIG_POWEROFF 3U
