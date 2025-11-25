/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pebble_tasks.h"

#include "FreeRTOS.h"
#include "task.h"

PebbleTask pebble_task_get_current(void) {
  return 0;
}

TaskHandle_t pebble_task_get_handle_for_task(PebbleTask task) {
  return NULL;
}

const char* pebble_task_get_name(PebbleTask task) {
  return NULL;
}

void pebble_task_unregister(PebbleTask task) {
}

void pebble_task_create(PebbleTask pebble_task, TaskParameters_t *task_params,
                        TaskHandle_t *handle) {
}
