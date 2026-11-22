/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */

#ifndef _DT_BINDINGS_DMA_SIFLI_SF32LB57_GPDMA_H
#define _DT_BINDINGS_DMA_SIFLI_SF32LB57_GPDMA_H

/* Channel priorities, the third specifier cell */
#define SF32LB57_DMA_PRIO_LOW       0
#define SF32LB57_DMA_PRIO_MEDIUM    1
#define SF32LB57_DMA_PRIO_HIGH      2
#define SF32LB57_DMA_PRIO_VERY_HIGH 3

/* GPDMA1 (HPSYS) request lines, the second specifier cell */
#define SF32LB57_GPDMA1_REQ_USART1_RX 5
#define SF32LB57_GPDMA1_REQ_SDMMC1    57
#define SF32LB57_GPDMA1_REQ_SDMMC2    59

#endif /* _DT_BINDINGS_DMA_SIFLI_SF32LB57_GPDMA_H */
