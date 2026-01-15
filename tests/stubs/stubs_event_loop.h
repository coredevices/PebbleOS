/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/event_loop.h"

#ifndef LAUNCHER_TASK_ADD_CALLBACK_PROVIDED
// Some tests provide their own implementation of this function
void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  callback(data);
}
#endif

bool launcher_task_is_current_task(void) {
  return false;
}
