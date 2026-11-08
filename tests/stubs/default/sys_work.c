/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/work.h>

#include <unistd.h>

PBL_WEAK bool pbl_sys_work_call(pbl_work_call_fn_t cb, void *data) {
  cb(data);
  return true;
}

PBL_WEAK size_t pbl_sys_work_num_free(void) {
  return 0;
}
