/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/system_task.h>

#include <unistd.h>

PBL_WEAK bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  cb(data);
  return true;
}

PBL_WEAK uint32_t system_task_get_available_space(void) {
  return 0;
}
