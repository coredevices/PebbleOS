/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/event_loop.h"

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  callback(data);
}
