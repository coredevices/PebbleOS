/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32H7_DMAMUX_CCR_OFFSET(c) (0x0000 + 0x4 * (c))

#define DMAMUX_CCR_DMAREQ_ID_MASK PBL_GENMASK(7, 0)
