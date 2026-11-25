/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_GRTC_TASKS_START_OFFSET          0x0060
#define NRF54L_GRTC_TASKS_STOP_OFFSET           0x0064
#define NRF54L_GRTC_EVENTS_COMPARE_OFFSET(n)    (0x0100 + ((n) << 2))
#define NRF54L_GRTC_INTENSET_OFFSET(g)          (0x0304 + ((g) << 4))
#define NRF54L_GRTC_INTENCLR_OFFSET(g)          (0x0308 + ((g) << 4))
#define NRF54L_GRTC_MODE_OFFSET                 0x0510
#define NRF54L_GRTC_CC_CCL_OFFSET(n)            (0x0520 + ((n) << 4))
#define NRF54L_GRTC_CC_CCH_OFFSET(n)            (0x0524 + ((n) << 4))
#define NRF54L_GRTC_CC_CCEN_OFFSET(n)           (0x052c + ((n) << 4))
#define NRF54L_GRTC_TIMEOUT_OFFSET              0x06a4
#define NRF54L_GRTC_WAKETIME_OFFSET             0x06ac
#define NRF54L_GRTC_CLKCFG_OFFSET               0x0718
#define NRF54L_GRTC_SYSCOUNTERL_OFFSET(n)       (0x0720 + ((n) << 4))
#define NRF54L_GRTC_SYSCOUNTERH_OFFSET(n)       (0x0724 + ((n) << 4))
#define NRF54L_GRTC_SYSCOUNTER_ACTIVE_OFFSET(n) (0x0728 + ((n) << 4))

/* Register bit definitions */

#define GRTC_INT_COMPARE(n) PBL_BIT(n)

#define GRTC_MODE_AUTOEN       PBL_BIT(0)
#define GRTC_MODE_SYSCOUNTEREN PBL_BIT(1)

#define GRTC_CC_CCH_MASK    PBL_GENMASK(19, 0)
#define GRTC_CC_CCEN_ACTIVE PBL_BIT(0)

#define GRTC_CLKCFG_CLKSEL_MASK  PBL_GENMASK(17, 16)
#define GRTC_CLKCFG_CLKSEL_LFCLK 1U

#define GRTC_SYSCOUNTERH_BUSY     PBL_BIT(30)
#define GRTC_SYSCOUNTERH_OVERFLOW PBL_BIT(31)

#define GRTC_SYSCOUNTER_ACTIVE PBL_BIT(0)
