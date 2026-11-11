/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define NRF52_UICR_PSELRESET_OFFSET(n) (0x0200 + (n) * 4)
#define NRF52_UICR_APPROTECT_OFFSET    0x0208
#define NRF52_UICR_NFCPINS_OFFSET      0x020C

/* Register bit definitions */

#define UICR_PSELRESET_DISCONNECTED PBL_BIT(31)

#define UICR_NFCPINS_PROTECT_NFC PBL_BIT(0)
