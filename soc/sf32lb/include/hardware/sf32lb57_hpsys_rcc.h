/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/*
 * The SF32LB57 HPSYS RCC is the SF32LB52 one (sf32lb52_hpsys_rcc.h) without CSR.SEL_SYS_LP,
 * plus the gates of its own blocks and these fields.
 */

#define HPSYS_RCC_CSR_SEL_MPI3_MASK PBL_GENMASK(9, 8)

#define SF32LB57_HPSYS_RCC_DLLCR_IN_DIV2_EN  PBL_BIT(13)
#define SF32LB57_HPSYS_RCC_DLLCR_OUT_DIV2_EN PBL_BIT(14)
