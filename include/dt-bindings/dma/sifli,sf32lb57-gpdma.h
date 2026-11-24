/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */

#pragma once

/* Channel priorities, the third specifier cell */
#define SF32LB57_DMA_PRIO_LOW       0
#define SF32LB57_DMA_PRIO_MEDIUM    1
#define SF32LB57_DMA_PRIO_HIGH      2
#define SF32LB57_DMA_PRIO_VERY_HIGH 3

/* GPDMA1 (HPSYS) request lines, the second specifier cell */
#define SF32LB57_GPDMA1_REQ_USART1_RX 5
#define SF32LB57_GPDMA1_REQ_SPI1_TX   28
#define SF32LB57_GPDMA1_REQ_SPI1_RX   29
#define SF32LB57_GPDMA1_REQ_SPI2_TX   30
#define SF32LB57_GPDMA1_REQ_SPI2_RX   31
