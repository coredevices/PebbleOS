/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets, the C1 ones being those of the Cortex-M7 */

#define STM32H7_EXTI_RTSR1_OFFSET  0x0000
#define STM32H7_EXTI_FTSR1_OFFSET  0x0004
#define STM32H7_EXTI_C1IMR1_OFFSET 0x0080
#define STM32H7_EXTI_C1PR1_OFFSET  0x0088

/* SYSCFG register offsets */

#define STM32H7_SYSCFG_EXTICR_OFFSET(line) (0x0008 + ((line) / 4U) * 4U)

#define SYSCFG_EXTICR_MASK(line) (0xfU << (((line) % 4U) * 4U))
