/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_RRAMC_POWER_LOWPOWERCONFIG_OFFSET 0x0518
// Glitch detector, sharing the registers of the RRAMC
#define NRF54L_GLITCHDET_CONFIG_OFFSET 0x05a0

/* Register bit definitions */

#define RRAMC_POWER_LOWPOWERCONFIG_POWEROFF 3U
