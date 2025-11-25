/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "FreeRTOS.h"
#include "queue.h"

signed portBASE_TYPE xQueueGenericReceive( QueueHandle_t pxQueue, void * const pvBuffer, TickType_t xTicksToWait, portBASE_TYPE xJustPeeking ) {
  return pdTRUE;
}

signed portBASE_TYPE xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, portBASE_TYPE xCopyPosition ) {
  return pdTRUE;
}

QueueHandle_t xQueueGenericCreate( unsigned portBASE_TYPE uxQueueLength, unsigned portBASE_TYPE uxItemSize, unsigned char ucQueueType ) {
  return (void *)(intptr_t) -1;
}

void vQueueDelete( QueueHandle_t xQueue ) {
}

QueueHandle_t xQueueCreateMutex( unsigned char ucQueueType ) {
  return (QueueHandle_t)1;
}

portBASE_TYPE xQueueTakeMutexRecursive( QueueHandle_t pxMutex, TickType_t xBlockTime ) {
  return pdTRUE;
}

portBASE_TYPE xQueueGiveMutexRecursive( QueueHandle_t xMutex ) {
  return pdTRUE;
}

BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue ) {
  return pdTRUE;
}

