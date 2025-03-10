/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "kernel/memory_layout.h"

#include <stdint.h>

#include "freertos_types.h"

//! This is an enumeration of different tasks we've had in our system. Please don't rearrange
//! these numbers! For example, the value of PebbleTask_Timers is hardcoded into our syscall
//! assembly and terrible things will happen if you move this around.
typedef enum PebbleTask {
  PebbleTask_KernelMain,
  PebbleTask_KernelBackground,
  PebbleTask_Worker,
  PebbleTask_App,

  PebbleTask_BTHost,        // Bluetooth Host
  PebbleTask_BTController,  // Bluetooth Controller
  PebbleTask_BTHCI,         // Bluetooth HCI

  PebbleTask_NewTimers,

  PebbleTask_PULSE,

  NumPebbleTask,

  PebbleTask_Unknown
} PebbleTask;

typedef uint16_t PebbleTaskBitset;

_Static_assert((1 << (8*sizeof(PebbleTaskBitset))) >= (1 << NumPebbleTask),
               "The type of PebbleTaskBitset is not wide enough to "
               "track all tasks in the PebbleTask enum");

void pebble_task_register(PebbleTask task, TaskHandle_t task_handle);
void pebble_task_unregister(PebbleTask task);

const char* pebble_task_get_name(PebbleTask task);

//! @return a single character that indicates the task
char pebble_task_get_char(PebbleTask task);

PebbleTask pebble_task_get_current(void);

PebbleTask pebble_task_get_task_for_handle(TaskHandle_t task_handle);
TaskHandle_t pebble_task_get_handle_for_task(PebbleTask task);

void pebble_task_suspend(PebbleTask task);

//! @return The queue handle to send events to the given task.
QueueHandle_t pebble_task_get_to_queue(PebbleTask task);

void pebble_task_create(PebbleTask pebble_task, TaskParameters_t *task_params,
                        TaskHandle_t *handle);

void pebble_task_configure_idle_task(void);
