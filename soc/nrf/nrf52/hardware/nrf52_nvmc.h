/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_NVMC_ICACHECNF_OFFSET 0x0540

/* Register bit definitions */

#define NVMC_ICACHECNF_CACHEEN PBL_BIT(0)
