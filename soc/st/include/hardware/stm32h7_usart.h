/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Register offsets */

#define STM32H7_USART_CR1_OFFSET   0x0000
#define STM32H7_USART_CR2_OFFSET   0x0004
#define STM32H7_USART_CR3_OFFSET   0x0008
#define STM32H7_USART_BRR_OFFSET   0x000c
#define STM32H7_USART_RQR_OFFSET   0x0018
#define STM32H7_USART_ISR_OFFSET   0x001c
#define STM32H7_USART_ICR_OFFSET   0x0020
#define STM32H7_USART_RDR_OFFSET   0x0024
#define STM32H7_USART_TDR_OFFSET   0x0028
#define STM32H7_USART_PRESC_OFFSET 0x002c

/* Register bit definitions */

#define USART_CR1_UE     PBL_BIT(0)
#define USART_CR1_RE     PBL_BIT(2)
#define USART_CR1_TE     PBL_BIT(3)
#define USART_CR1_RXNEIE PBL_BIT(5)
#define USART_CR1_PS     PBL_BIT(9)
#define USART_CR1_PCE    PBL_BIT(10)
#define USART_CR1_M0     PBL_BIT(12)

#define USART_CR3_RTSE PBL_BIT(8)
#define USART_CR3_CTSE PBL_BIT(9)

#define USART_RQR_RXFRQ PBL_BIT(3)

#define USART_ISR_PE    PBL_BIT(0)
#define USART_ISR_FE    PBL_BIT(1)
#define USART_ISR_NE    PBL_BIT(2)
#define USART_ISR_ORE   PBL_BIT(3)
#define USART_ISR_RXNE  PBL_BIT(5)
#define USART_ISR_TC    PBL_BIT(6)
#define USART_ISR_TXE   PBL_BIT(7)
#define USART_ISR_TEACK PBL_BIT(21)

#define USART_ICR_PECF  PBL_BIT(0)
#define USART_ICR_FECF  PBL_BIT(1)
#define USART_ICR_NECF  PBL_BIT(2)
#define USART_ICR_ORECF PBL_BIT(3)

#define USART_BRR_MIN 0x10U
#define USART_BRR_MAX 0xffffU
