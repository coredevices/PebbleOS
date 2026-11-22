/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets: the C1 ones being those of the Cortex-M7, the C2 ones those of the Cortex-M4 */

#define STM32H7_HSEM_R_OFFSET(n)   (0x0000 + 4U * (n))
#define STM32H7_HSEM_RLR_OFFSET(n) (0x0080 + 4U * (n))
#define STM32H7_HSEM_C1IER_OFFSET  0x0100
#define STM32H7_HSEM_C1ICR_OFFSET  0x0104
#define STM32H7_HSEM_C1ISR_OFFSET  0x0108
#define STM32H7_HSEM_C1MISR_OFFSET 0x010c
#define STM32H7_HSEM_C2IER_OFFSET  0x0110
#define STM32H7_HSEM_C2ICR_OFFSET  0x0114
#define STM32H7_HSEM_C2ISR_OFFSET  0x0118
#define STM32H7_HSEM_C2MISR_OFFSET 0x011c

/* Register bit definitions */

#define HSEM_R_LOCK        PBL_BIT(31)
#define HSEM_R_COREID_MASK PBL_GENMASK(11, 8)
#define HSEM_R_PROCID_MASK PBL_GENMASK(7, 0)

/* The cores, as the semaphores know them */
#define HSEM_COREID_CM7 3U
#define HSEM_COREID_CM4 1U
