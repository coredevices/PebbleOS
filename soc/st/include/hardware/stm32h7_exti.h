/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets, the C1 ones being those of the Cortex-M7, the C2 ones those of the Cortex-M4 */

#define STM32H7_EXTI_RTSR1_OFFSET  0x0000
#define STM32H7_EXTI_FTSR1_OFFSET  0x0004
#define STM32H7_EXTI_C1IMR1_OFFSET 0x0080
#define STM32H7_EXTI_C1PR1_OFFSET  0x0088
#define STM32H7_EXTI_C2IMR1_OFFSET 0x00c0
#define STM32H7_EXTI_C2PR1_OFFSET  0x00c8

/* Those of the core running the code */
#ifdef CONFIG_CPU_CORTEX_M4
#define STM32H7_EXTI_CORE_IMR1_OFFSET STM32H7_EXTI_C2IMR1_OFFSET
#define STM32H7_EXTI_CORE_PR1_OFFSET  STM32H7_EXTI_C2PR1_OFFSET
#else
#define STM32H7_EXTI_CORE_IMR1_OFFSET STM32H7_EXTI_C1IMR1_OFFSET
#define STM32H7_EXTI_CORE_PR1_OFFSET  STM32H7_EXTI_C1PR1_OFFSET
#endif

/* The NVIC interrupt of the configurable lines a device interrupts through */
#define STM32H7_EXTI_IRQ(line)  STM32H7_EXTI_IRQ_(line)
#define STM32H7_EXTI_IRQ_(line) STM32H7_EXTI_LINE##line##_IRQ
#define STM32H7_EXTI_LINE17_IRQ 41

/* SYSCFG register offsets */

#define STM32H7_SYSCFG_EXTICR_OFFSET(line) (0x0008 + ((line) / 4U) * 4U)

#define SYSCFG_EXTICR_MASK(line) (0xfU << (((line) % 4U) * 4U))
