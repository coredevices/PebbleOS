/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets, the C1 ones being those of the Cortex-M7 */

#define STM32H7_EXTI_RTSR1_OFFSET  0x0000
#define STM32H7_EXTI_FTSR1_OFFSET  0x0004
#define STM32H7_EXTI_C1IMR1_OFFSET 0x0080
#define STM32H7_EXTI_C1EMR1_OFFSET 0x0084
#define STM32H7_EXTI_C1PR1_OFFSET  0x0088
#define STM32H7_EXTI_C1IMR2_OFFSET 0x0090
#define STM32H7_EXTI_C1EMR2_OFFSET 0x0094
#define STM32H7_EXTI_C1PR2_OFFSET  0x0098
#define STM32H7_EXTI_C1IMR3_OFFSET 0x00a0
#define STM32H7_EXTI_C1EMR3_OFFSET 0x00a4
#define STM32H7_EXTI_C1PR3_OFFSET  0x00a8

/* Lines 55 to 60, of the PWR wakeup pins, in the second set of registers */
#define EXTI_LINES2_WKUP PBL_GENMASK(28, 23)

/* SYSCFG register offsets */

#define STM32H7_SYSCFG_EXTICR_OFFSET(line) (0x0008 + ((line) / 4U) * 4U)

#define SYSCFG_EXTICR_MASK(line) (0xfU << (((line) % 4U) * 4U))
