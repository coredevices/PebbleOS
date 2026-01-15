/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/event_loop.h"

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  callback(data);
}

bool launcher_task_is_current_task(void) {
  return false;
}
