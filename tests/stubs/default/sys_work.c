/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/sys_work.h>

#include <unistd.h>

PBL_WEAK bool sys_work_add(SysWorkCallback cb, void *data) {
  cb(data);
  return true;
}

PBL_WEAK uint32_t sys_work_get_available_space(void) {
  return 0;
}
