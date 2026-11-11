/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF52_NVMC_READY_OFFSET     0x0400
#define NRF52_NVMC_CONFIG_OFFSET    0x0504
#define NRF52_NVMC_ICACHECNF_OFFSET 0x0540

/* Register bit definitions */

#define NVMC_READY_READY PBL_BIT(0)

#define NVMC_CONFIG_WEN_REN 0U
#define NVMC_CONFIG_WEN_WEN 1U

#define NVMC_ICACHECNF_CACHEEN PBL_BIT(0)
