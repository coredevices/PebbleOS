/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdbool.h>

#include "drivers/dma.h"

T_STATIC DMADirectRequestHandler handlers[DMA_CHANNEL_NUM];

static void dma_cplt_callback(DMA_HandleTypeDef *hdma)
{

}

void dma_request_init(DMARequest *this)
{

}

void dma_request_start_direct(DMARequest *this, void *dst, const void *src, uint32_t length,
                              DMADirectRequestHandler handler, void *context)
{

}

void dma_request_start_circular(DMARequest *this, void *dst, const void *src, uint32_t length,
                                DMACircularRequestHandler handler, void *context)
{

}

void dma_request_stop(DMARequest *this)
{

}

bool dma_request_in_progress(DMARequest *this)
{
  return true;
}

uint32_t dma_request_get_current_data_counter(DMARequest *this)
{
  return 0;
}

bool dma_request_get_and_clear_transfer_error(DMARequest *this);
{
  return true;
}

void dma_request_set_memory_increment_disabled(DMARequest *this, bool disabled)
{

}
