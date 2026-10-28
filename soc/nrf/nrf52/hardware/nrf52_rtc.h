/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

#define NRF52_RTC_CLOCK_HZ 32768U

/* Register offsets */

#define NRF52_RTC_TASKS_START_OFFSET       0x0000
#define NRF52_RTC_TASKS_STOP_OFFSET        0x0004
#define NRF52_RTC_TASKS_CLEAR_OFFSET       0x0008
#define NRF52_RTC_EVENTS_COMPARE_OFFSET(n) (0x0140 + ((n) << 2))
#define NRF52_RTC_INTENCLR_OFFSET          0x0308
#define NRF52_RTC_EVTEN_OFFSET             0x0340
#define NRF52_RTC_PRESCALER_OFFSET         0x0508
#define NRF52_RTC_CC_OFFSET(n)             (0x0540 + ((n) << 2))

/* Register bit definitions */

#define RTC_EVT_COMPARE(n) PBL_BIT(16 + (n))

#define RTC_COUNTER_MASK PBL_GENMASK(23, 0)
