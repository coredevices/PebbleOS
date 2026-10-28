/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_TIMER_TASKS_START_OFFSET      0x0000
#define NRF52_TIMER_TASKS_STOP_OFFSET       0x0004
#define NRF52_TIMER_TASKS_COUNT_OFFSET      0x0008
#define NRF52_TIMER_TASKS_CLEAR_OFFSET      0x000c
#define NRF52_TIMER_TASKS_SHUTDOWN_OFFSET   0x0010
#define NRF52_TIMER_TASKS_CAPTURE_OFFSET(n) (0x0040 + ((n) * 4))
#define NRF52_TIMER_MODE_OFFSET             0x0504
#define NRF52_TIMER_BITMODE_OFFSET          0x0508
#define NRF52_TIMER_CC_OFFSET(n)            (0x0540 + ((n) * 4))

/* Register bit definitions */

#define TIMER_MODE_LOW_POWER_COUNTER 2U

#define TIMER_BITMODE_32BIT 3U
