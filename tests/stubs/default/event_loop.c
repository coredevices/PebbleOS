/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <kernel/event_loop.h>

PBL_WEAK void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  callback(data);
}
