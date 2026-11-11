/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define SF32LB52_TSEN_CTRL_OFFSET  0x0000
#define SF32LB52_TSEN_RDATA_OFFSET 0x0004
#define SF32LB52_TSEN_IRQ_OFFSET   0x0008

/* Register bit definitions */

#define TSEN_CTRL_PU           PBL_BIT(0)
#define TSEN_CTRL_RSTB         PBL_BIT(1)
#define TSEN_CTRL_RUN          PBL_BIT(2)
#define TSEN_CTRL_IG_VBE_MASK  PBL_GENMASK(5, 3)
#define TSEN_CTRL_FCK_SEL_MASK PBL_GENMASK(7, 6)
#define TSEN_CTRL_SGN_EN       PBL_BIT(8)
#define TSEN_CTRL_SER_PAR_SEL  PBL_BIT(9)
#define TSEN_CTRL_RDY          PBL_BIT(10)
#define TSEN_CTRL_EN           PBL_BIT(11)
#define TSEN_CTRL_CLK_DIV_MASK PBL_GENMASK(17, 12)

#define TSEN_RDATA_MASK PBL_GENMASK(11, 0)

#define TSEN_IRQ_ICR  PBL_BIT(0)
#define TSEN_IRQ_IMR  PBL_BIT(1)
#define TSEN_IRQ_IRSR PBL_BIT(2)
#define TSEN_IRQ_ISR  PBL_BIT(3)
