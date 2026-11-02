/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

/* Register offsets */

#define NRF52_PDM_TASKS_START_OFFSET    0x0000
#define NRF52_PDM_TASKS_STOP_OFFSET     0x0004
#define NRF52_PDM_EVENTS_STARTED_OFFSET 0x0100
#define NRF52_PDM_EVENTS_STOPPED_OFFSET 0x0104
#define NRF52_PDM_EVENTS_END_OFFSET     0x0108
#define NRF52_PDM_INTENSET_OFFSET       0x0304
#define NRF52_PDM_INTENCLR_OFFSET       0x0308
#define NRF52_PDM_ENABLE_OFFSET         0x0500
#define NRF52_PDM_PDMCLKCTRL_OFFSET     0x0504
#define NRF52_PDM_MODE_OFFSET           0x0508
#define NRF52_PDM_GAINL_OFFSET          0x0518
#define NRF52_PDM_GAINR_OFFSET          0x051c
#define NRF52_PDM_RATIO_OFFSET          0x0520
#define NRF52_PDM_PSEL_CLK_OFFSET       0x0540
#define NRF52_PDM_PSEL_DIN_OFFSET       0x0544
#define NRF52_PDM_SAMPLE_PTR_OFFSET     0x0560
#define NRF52_PDM_SAMPLE_MAXCNT_OFFSET  0x0564

/* Register bit definitions */

#define PDM_INT_STARTED PBL_BIT(0)
#define PDM_INT_STOPPED PBL_BIT(1)
#define PDM_INT_END     PBL_BIT(2)

#define PDM_ENABLE_ENABLED 1U

#define PDM_PDMCLKCTRL_FREQ_1280K 0x0a000000U

#define PDM_MODE_MONO PBL_BIT(0)

/* 0.5 dB steps from -20 dB */
#define PDM_GAIN_MIN 0x00U
#define PDM_GAIN_MAX 0x50U

#define PDM_RATIO_80 1U

#define PDM_PSEL_DISCONNECTED PBL_BIT(31)

/* In 16-bit samples */
#define PDM_SAMPLE_MAXCNT_MAX 0x7fffU
