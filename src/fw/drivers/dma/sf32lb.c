/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdbool.h>

#include "board/board.h"

#include <bf0_hal.h>

#include "sf32lb.h"

#define DMA_CHANNEL_NUM 8U

struct DMARequest {
  DMA_HandleTypeDef *handle;
};

static DMADirectRequestHandler handlers[DMA_CHANNEL_NUM];
static DMACircularRequestHandler circular_handlers[DMA_CHANNEL_NUM];

static void dma_cplt_callback(DMA_HandleTypeDef *hdma)
{
  DMARequest *this = (DMARequest *)hdma->Parent; //container_of?

  if (handlers[hdma->ChannelIndex] != NULL)
    handlers[hdma->ChannelIndex](this, NULL);
}

void dma_request_init(DMARequest *this)
{
  HAL_DMA_Init(this->handle);
}

void dma_request_start_direct(DMARequest *this, void *dst, const void *src, uint32_t length,
                              DMADirectRequestHandler handler, void *context)
{
  HAL_DMA_RegisterCallback(this->handle, HAL_DMA_XFER_CPLT_CB_ID, dma_cplt_callback);
  handlers[this->handle->ChannelIndex] = handler;
  HAL_DMA_Start_IT(this->handle, (uint32_t)src, (uint32_t)dst, length);
}

void dma_request_start_circular(DMARequest *this, void *dst, const void *src, uint32_t length,
                                DMACircularRequestHandler handler, void *context)
{
  HAL_DMA_RegisterCallback(this->handle, HAL_DMA_XFER_CPLT_CB_ID, dma_cplt_callback);
  circular_handlers[this->handle->ChannelIndex] = handler;
  HAL_DMA_Start_IT(this->handle, (uint32_t)src, (uint32_t)dst, length);
}

void dma_request_stop(DMARequest *this)
{
  HAL_DMA_Abort(this->handle);
}

bool dma_request_in_progress(DMARequest *this)
{
  return (HAL_DMA_GetState(this->handle) == HAL_DMA_STATE_BUSY);
}

uint32_t dma_request_get_current_data_counter(DMARequest *this)
{
  return 0;
//  return HAL_DMA_GetCurrentDataCounter(this->handle);
}

bool dma_request_get_and_clear_transfer_error(DMARequest *this)
{
  return false;
}

void dma_request_set_memory_increment_disabled(DMARequest *this, bool disabled)
{

}
