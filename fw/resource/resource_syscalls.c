/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource.h"

#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>

#include <process_management/app_manager.h>
#include <process_management/worker_manager.h>

//! @file resource_syscalls.c
//! The landing place for untrusted code to use resources.

static void prv_assert_app_num_accessible(ResAppNum app_num) {
  if (app_num == SYSTEM_APP) {
    return;
  }

  const PebbleProcessMd *md = (pebble_task_get_current() == PebbleTask_Worker)
                                  ? worker_manager_get_current_worker_md()
                                  : app_manager_get_current_app_md();
  if (md == nullptr || app_num != (ResAppNum)process_metadata_get_res_bank_num(md)) {
    pbl_syscall_fail();
  }
}

PBL_SYSCALL(size_t, sys_resource_size, (VAL, ResAppNum, app_num), (VAL, uint32_t, resource_id)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  if (from_user) {
    prv_assert_app_num_accessible(app_num);
  }

  return resource_size(app_num, resource_id);
}

PBL_SYSCALL(size_t, sys_resource_load_range, (VAL, ResAppNum, app_num), (VAL, uint32_t, id),
            (VAL, uint32_t, start_offset), (OUT_BUF, uint8_t *, data, num_bytes),
            (VAL, size_t, num_bytes)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  if (from_user) {
    prv_assert_app_num_accessible(app_num);
  }

  return resource_load_byte_range_system(app_num, id, start_offset, data, num_bytes);
}

PBL_SYSCALL(bool, sys_resource_bytes_are_readonly, (VAL, void *, ptr)) {
  return resource_bytes_are_readonly(ptr);
}

PBL_SYSCALL(const uint8_t *, sys_resource_read_only_bytes, (VAL, ResAppNum, app_num),
            (VAL, uint32_t, resource_id), (OUT_OPT, size_t *, num_bytes_out)) {
  if (from_user && pebble_task_get_current() == PebbleTask_Worker) {
    // Not allowed from workers
    pbl_syscall_fail();
  }
  if (from_user) {
    prv_assert_app_num_accessible(app_num);
  }

  return resource_get_readonly_bytes(app_num, resource_id, num_bytes_out, !from_user);
}

PBL_SYSCALL(bool, sys_resource_is_valid, (VAL, ResAppNum, app_num), (VAL, uint32_t, resource_id)) {
  if (from_user) {
    prv_assert_app_num_accessible(app_num);
  }

  return resource_is_valid(app_num, resource_id);
}

PBL_SYSCALL(uint32_t, sys_resource_get_and_cache, (VAL, ResAppNum, app_num),
            (VAL, uint32_t, resource_id)) {
  if (from_user && app_num != SYSTEM_APP) {
    pbl_syscall_fail();
  }

  return resource_get_and_cache(app_num, resource_id);
}
