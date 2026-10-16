/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_TEMP_TASKS_START_OFFSET    0x0000
#define NRF52_TEMP_TASKS_STOP_OFFSET     0x0004
#define NRF52_TEMP_EVENTS_DATARDY_OFFSET 0x0100
#define NRF52_TEMP_INTENSET_OFFSET       0x0304
#define NRF52_TEMP_INTENCLR_OFFSET       0x0308
#define NRF52_TEMP_TEMP_OFFSET           0x0508
