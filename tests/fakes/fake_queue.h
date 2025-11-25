/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "FreeRTOS.h"
#include "queue.h"

#include <stdbool.h>

void fake_queue_set_yield_callback(QueueHandle_t queue,
                                   TickType_t (*yield_cb)(QueueHandle_t));
