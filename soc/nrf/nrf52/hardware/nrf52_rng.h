/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_RNG_TASKS_START_OFFSET   0x0000
#define NRF52_RNG_TASKS_STOP_OFFSET    0x0004
#define NRF52_RNG_EVENTS_VALRDY_OFFSET 0x0100
#define NRF52_RNG_SHORTS_OFFSET        0x0200
#define NRF52_RNG_INTENSET_OFFSET      0x0304
#define NRF52_RNG_INTENCLR_OFFSET      0x0308
#define NRF52_RNG_CONFIG_OFFSET        0x0504
#define NRF52_RNG_VALUE_OFFSET         0x0508

/* Register bit definitions */

#define RNG_SHORTS_VALRDY_STOP PBL_BIT(0)

#define RNG_INT_VALRDY PBL_BIT(0)

#define RNG_CONFIG_DERCEN PBL_BIT(0)

#define RNG_VALUE_MASK PBL_GENMASK(7, 0)
