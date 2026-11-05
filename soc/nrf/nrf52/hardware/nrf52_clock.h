/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_CLOCK_TASKS_LFCLKSTART_OFFSET    0x0008
#define NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET 0x0104
#define NRF52_CLOCK_LFCLKSRC_OFFSET            0x0518

/* Register bit definitions */

#define CLOCK_LFCLKSRC_SRC_XTAL 1U
