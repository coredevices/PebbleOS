/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF54L_TAMPC_DOMAIN_DBGEN_CTRL_OFFSET   0x0500
#define NRF54L_TAMPC_DOMAIN_NIDEN_CTRL_OFFSET   0x0508
#define NRF54L_TAMPC_DOMAIN_SPIDEN_CTRL_OFFSET  0x0510
#define NRF54L_TAMPC_DOMAIN_SPNIDEN_CTRL_OFFSET 0x0518
#define NRF54L_TAMPC_AP_DBGEN_CTRL_OFFSET       0x0700

/* Register bit definitions */

#define TAMPC_CTRL_VALUE_HIGH          PBL_BIT(0)
#define TAMPC_CTRL_LOCK                PBL_BIT(1)
#define TAMPC_CTRL_WRITEPROTECTION_CLR PBL_FIELD_PREP(PBL_GENMASK(7, 4), 0xfU)
#define TAMPC_CTRL_KEY                 PBL_FIELD_PREP(PBL_GENMASK(31, 16), 0x50faU)
