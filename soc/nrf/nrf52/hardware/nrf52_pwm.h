/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/nrf52_memorymap.h>

#define NRF52_PWM_NUM_CHANNELS 4U

/* Register offsets */

#define NRF52_PWM_TASKS_STOP_OFFSET      0x0004
#define NRF52_PWM_TASKS_SEQSTART0_OFFSET 0x0008
#define NRF52_PWM_EVENTS_STOPPED_OFFSET  0x0104
#define NRF52_PWM_SHORTS_OFFSET          0x0200
#define NRF52_PWM_INTEN_OFFSET           0x0300
#define NRF52_PWM_ENABLE_OFFSET          0x0500
#define NRF52_PWM_MODE_OFFSET            0x0504
#define NRF52_PWM_COUNTERTOP_OFFSET      0x0508
#define NRF52_PWM_PRESCALER_OFFSET       0x050c
#define NRF52_PWM_DECODER_OFFSET         0x0510
#define NRF52_PWM_LOOP_OFFSET            0x0514
#define NRF52_PWM_SEQ0_PTR_OFFSET        0x0520
#define NRF52_PWM_SEQ0_CNT_OFFSET        0x0524
#define NRF52_PWM_SEQ0_REFRESH_OFFSET    0x0528
#define NRF52_PWM_SEQ0_ENDDELAY_OFFSET   0x052c
#define NRF52_PWM_PSEL_OUT_OFFSET(n)     (0x0560 + ((n) << 2))

/* Register bit definitions */

#define PWM_ENABLE_ENABLED 1U

#define PWM_MODE_UP 0U

#define PWM_COUNTERTOP_MIN 3U
#define PWM_COUNTERTOP_MAX 32767U

#define PWM_PRESCALER_MAX 7U

#define PWM_DECODER_LOAD_INDIVIDUAL 2U

#define PWM_PSEL_DISCONNECTED PBL_BIT(31)

/* Sequence value: compare value, and the first edge of the period falling instead of rising */

#define PWM_SEQ_COMPARE_MASK PBL_GENMASK(14, 0)
#define PWM_SEQ_FALLING_EDGE PBL_BIT(15)
