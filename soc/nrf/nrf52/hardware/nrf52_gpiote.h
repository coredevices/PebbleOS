/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

#define NRF52_GPIOTE_NUM_CHANNELS 8U

/* Register offsets */

#define NRF52_GPIOTE_TASKS_SET_OFFSET(n) (0x0030 + ((n) << 2))
#define NRF52_GPIOTE_TASKS_CLR_OFFSET(n) (0x0060 + ((n) << 2))
#define NRF52_GPIOTE_EVENTS_IN_OFFSET(n) (0x0100 + ((n) << 2))
#define NRF52_GPIOTE_INTENSET_OFFSET     0x0304
#define NRF52_GPIOTE_INTENCLR_OFFSET     0x0308
#define NRF52_GPIOTE_CONFIG_OFFSET(n)    (0x0510 + ((n) << 2))

/* Register bit definitions */

#define GPIOTE_INT_IN(n) PBL_BIT(n)

#define GPIOTE_CONFIG_MODE_MASK       PBL_GENMASK(1, 0)
#define GPIOTE_CONFIG_MODE_EVENT      1U
#define GPIOTE_CONFIG_MODE_TASK       3U
#define GPIOTE_CONFIG_PSEL_MASK       PBL_GENMASK(12, 8)
#define GPIOTE_CONFIG_PORT            PBL_BIT(13)
#define GPIOTE_CONFIG_POLARITY_MASK   PBL_GENMASK(17, 16)
#define GPIOTE_CONFIG_POLARITY_LOTOHI 1U
#define GPIOTE_CONFIG_POLARITY_HITOLO 2U
#define GPIOTE_CONFIG_POLARITY_TOGGLE 3U
#define GPIOTE_CONFIG_OUTINIT_HIGH    PBL_BIT(20)
