/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "freertos_types.h"
#include "projdefs.h"

static uint32_t s_app_task_control_reg = 0;

void vTaskDelete(TaskHandle_t xTaskToDelete) {
  return;
}

void vTaskSuspend(TaskHandle_t xTaskToSuspend) {
  return;
}

void vTaskResume(TaskHandle_t xTaskToResume) {
  return;
}

TaskHandle_t xTaskGetCurrentTaskHandle(void) {
  return NULL;
}

uint32_t ulTaskDebugGetStackedControl(TaskHandle_t xTask) {
  return s_app_task_control_reg;
}

BaseType_t xTaskGenericCreate(TaskFunction_t pxTaskCode, const char * const pcName,
    const uint16_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority,
    TaskHandle_t * const pxCreatedTask, StackType_t * const puxStackBuffer,
    const MemoryRegion_t * const xRegions) {
  return pdTRUE;
}

// Stubs
////////////////////////////////////
void stub_control_reg(uint32_t reg) {
  s_app_task_control_reg = reg;
}
