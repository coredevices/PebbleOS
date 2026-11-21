/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#define STM32_IWDG_KR_OFFSET  0x0000
#define STM32_IWDG_PR_OFFSET  0x0004
#define STM32_IWDG_RLR_OFFSET 0x0008
#define STM32_IWDG_SR_OFFSET  0x000c

#define IWDG_KR_RELOAD 0xaaaaU
#define IWDG_KR_UNLOCK 0x5555U
#define IWDG_KR_START  0xccccU

#define IWDG_PR_MAX  6U
#define IWDG_RLR_MAX 0xfffU

#define IWDG_SR_PVU PBL_BIT(0)
#define IWDG_SR_RVU PBL_BIT(1)
