/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_WDT_TASKS_START_OFFSET    0x0000
#define NRF52_WDT_EVENTS_TIMEOUT_OFFSET 0x0100
#define NRF52_WDT_INTENSET_OFFSET       0x0304
#define NRF52_WDT_INTENCLR_OFFSET       0x0308
#define NRF52_WDT_RUNSTATUS_OFFSET      0x0400
#define NRF52_WDT_REQSTATUS_OFFSET      0x0404
#define NRF52_WDT_CRV_OFFSET            0x0504
#define NRF52_WDT_RREN_OFFSET           0x0508
#define NRF52_WDT_CONFIG_OFFSET         0x050c
#define NRF52_WDT_RR_OFFSET(n)          (0x0600 + ((n) << 2))

/* Register bit definitions */

#define WDT_INT_TIMEOUT PBL_BIT(0)

#define WDT_RUNSTATUS_RUNNING PBL_BIT(0)

#define WDT_REQSTATUS_RR(n) PBL_BIT(n)

#define WDT_RREN_RR(n) PBL_BIT(n)

#define WDT_CONFIG_SLEEP PBL_BIT(0)
#define WDT_CONFIG_HALT  PBL_BIT(3)

#define WDT_RR_RELOAD 0x6e524635UL
