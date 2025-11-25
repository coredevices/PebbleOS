/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/dma.h"
#include "kernel/util/stop.h"
#include "system/logging.h"

#include "FreeRTOS.h"
#include "semphr.h"

#if CAPABILITY_COMPOSITOR_USES_DMA && !TARGET_QEMU && !UNITTEST

static SemaphoreHandle_t s_dma_in_progress;

void compositor_dma_init(void) {
  s_dma_in_progress = xSemaphoreCreateBinary();
  dma_request_init(COMPOSITOR_DMA);
}

static bool prv_dma_complete_handler(DMARequest *transfer, void *context) {
  signed portBASE_TYPE should_context_switch = false;
  xSemaphoreGiveFromISR(s_dma_in_progress, &should_context_switch);
  return should_context_switch != pdFALSE;
}

void compositor_dma_run(void *to, const void *from, uint32_t size) {
  stop_mode_disable(InhibitorCompositor);
  dma_request_start_direct(COMPOSITOR_DMA, to, from, size, prv_dma_complete_handler, NULL);

  if (xSemaphoreTake(s_dma_in_progress, 10) != pdTRUE) {
    PBL_LOG_SYNC(LOG_LEVEL_ERROR, "DMA Compositing never completed.");
    // TODO: This should never be hit, but do we want to queue up a new render
    // event so that there is no visible breakage in low-fps situations?
    dma_request_stop(COMPOSITOR_DMA);
  }
  stop_mode_enable(InhibitorCompositor);
}

#endif // CAPABILITY_COMPOSITOR_USES_DMA && !TARGET_QEMU && !UNITTEST
