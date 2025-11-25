/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_queue.h"

#include "util/circular_buffer.h"

#include "FreeRTOS.h"
#include "queue.h"

#include <stdlib.h>

typedef struct {
  uint16_t item_size;
  bool is_semph;

  //! @return ticks spent executing the callback
  TickType_t (*yield_cb)(QueueHandle_t);
  CircularBuffer circular_buffer;
  uint8_t storage[];
} FakeQueue;

signed portBASE_TYPE xQueueGenericReceive(QueueHandle_t xQueue, void * const pvBuffer,
                                          TickType_t xTicksToWait, portBASE_TYPE xJustPeeking) {
  FakeQueue *q = (FakeQueue *) xQueue;
  TickType_t ticks_waited = 0;
  while (true) {
    uint16_t read_space = circular_buffer_get_read_space_remaining(&q->circular_buffer);
    if (read_space >= q->item_size) {
      circular_buffer_consume(&q->circular_buffer, q->item_size);
      return pdTRUE;
    } else {
      if (!xTicksToWait || !q->yield_cb) {
        return pdFALSE;
      } else {
        ticks_waited += q->yield_cb(xQueue);
        if (ticks_waited >= xTicksToWait) {
          return pdFALSE;
        }
      }
    }
  }
}

signed portBASE_TYPE xQueueGenericSend(QueueHandle_t xQueue, const void * const pvItemToQueue,
                                       TickType_t xTicksToWait, portBASE_TYPE xCopyPosition) {
  FakeQueue *q = (FakeQueue *) xQueue;
  TickType_t ticks_waited = 0;
  while (true) {
    uint16_t write_space = circular_buffer_get_write_space_remaining(&q->circular_buffer);
    if (write_space >= q->item_size) {
      // Just write a zero for a semaphore
      const uint8_t semph_data = 0;
      const uint8_t *data = q->is_semph ? &semph_data : (const uint8_t *) pvItemToQueue;
      circular_buffer_write(&q->circular_buffer, data, q->item_size);
      return pdTRUE;
    } else {
      if (!xTicksToWait || !q->yield_cb) {
        return pdFALSE;
      } else {
        ticks_waited += q->yield_cb(xQueue);
        if (ticks_waited >= xTicksToWait) {
          return pdFALSE;
        }
      }
    }
  }
}

QueueHandle_t xQueueGenericCreate(unsigned portBASE_TYPE uxQueueLength,
                                 unsigned portBASE_TYPE uxItemSize, unsigned char ucQueueType) {
  uint16_t item_size;
  const bool is_semph = (ucQueueType == queueQUEUE_TYPE_BINARY_SEMAPHORE);
  if (is_semph) {
    item_size = 1;
  } else {
    item_size = uxItemSize;
  }
  const uint16_t storage_size = (item_size * uxQueueLength);
  uint8_t *buffer = malloc(sizeof(FakeQueue) + storage_size);
  FakeQueue *q = (FakeQueue *) buffer;
  *q = (const FakeQueue) {
    .item_size = item_size,
    .is_semph = is_semph,
  };
  circular_buffer_init(&q->circular_buffer, q->storage, storage_size);
  return q;
}

void vQueueDelete(QueueHandle_t xQueue) {
  free(xQueue);
}

QueueHandle_t xQueueCreateMutex(unsigned char ucQueueType) {
  return (QueueHandle_t)1;
}

portBASE_TYPE xQueueTakeMutexRecursive(QueueHandle_t pxMutex, TickType_t xBlockTime) {
  return pdTRUE;
}

portBASE_TYPE xQueueGiveMutexRecursive(QueueHandle_t xMutex) {
  return pdTRUE;
}

BaseType_t xQueueGenericReset(QueueHandle_t xQueue, BaseType_t xNewQueue) {
  return pdTRUE;
}

void fake_queue_set_yield_callback(QueueHandle_t queue,
                                   TickType_t (*yield_cb)(QueueHandle_t)) {
  FakeQueue *fake_queue = (FakeQueue *) queue;
  fake_queue->yield_cb = yield_cb;
}
