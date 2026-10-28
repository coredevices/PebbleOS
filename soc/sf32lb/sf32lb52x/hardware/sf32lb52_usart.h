/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

#include <hardware/sf32lb52_memorymap.h>

/* Register offsets */

#define SF32LB52_USART_CR1_OFFSET   0x0000
#define SF32LB52_USART_CR2_OFFSET   0x0004
#define SF32LB52_USART_CR3_OFFSET   0x0008
#define SF32LB52_USART_BRR_OFFSET   0x000c
#define SF32LB52_USART_ISR_OFFSET   0x001c
#define SF32LB52_USART_ICR_OFFSET   0x0020
#define SF32LB52_USART_RDR_OFFSET   0x0024
#define SF32LB52_USART_TDR_OFFSET   0x0028
#define SF32LB52_USART_MISCR_OFFSET 0x002c

/* Register bit definitions */

#define UART_CR1_UE     PBL_BIT(0)
#define UART_CR1_RE     PBL_BIT(2)
#define UART_CR1_TE     PBL_BIT(3)
#define UART_CR1_IDLEIE PBL_BIT(4)
#define UART_CR1_RXNEIE PBL_BIT(5)
#define UART_CR1_PS     PBL_BIT(9)
#define UART_CR1_PCE    PBL_BIT(10)
#define UART_CR1_OVER8  PBL_BIT(14)
#define UART_CR1_M_MASK PBL_GENMASK(28, 27)
#define UART_CR1_M_8BIT 2U
#define UART_CR1_M_9BIT 3U

#define UART_CR2_STOP_MASK PBL_GENMASK(13, 12)

#define UART_CR3_DMAR PBL_BIT(6)
#define UART_CR3_RTSE PBL_BIT(8)
#define UART_CR3_CTSE PBL_BIT(9)

#define USART_BRR_MIN 0x10U

#define UART_ISR_PE   PBL_BIT(0)
#define UART_ISR_FE   PBL_BIT(1)
#define UART_ISR_NF   PBL_BIT(2)
#define UART_ISR_ORE  PBL_BIT(3)
#define UART_ISR_IDLE PBL_BIT(4)
#define UART_ISR_RXNE PBL_BIT(5)
#define UART_ISR_TC   PBL_BIT(6)
#define UART_ISR_TXE  PBL_BIT(7)

#define UART_ICR_PECF   PBL_BIT(0)
#define UART_ICR_FECF   PBL_BIT(1)
#define UART_ICR_NCF    PBL_BIT(2)
#define UART_ICR_ORECF  PBL_BIT(3)
#define UART_ICR_IDLECF PBL_BIT(4)

#define USART_RDR_MASK PBL_GENMASK(7, 0)

#define UART_MISCR_SMPLINI_MASK PBL_GENMASK(3, 0)
#define UART_MISCR_SMPLINI_16X  6U
#define UART_MISCR_SMPLINI_8X   2U
