/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource.h"

#include <pbl/logging/logging.h>

#include <process_management/app_manager.h>
#include <process_management/worker_manager.h>
#include <syscall/syscall_internal.h>

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
    syscall_failed();
  }
}

DEFINE_SYSCALL(size_t, sys_resource_size, ResAppNum app_num, uint32_t resource_id) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (pebble_task_get_current() == PebbleTask_Worker) {
      // Not allowed from workers
      syscall_failed();
    }

    prv_assert_app_num_accessible(app_num);
  }

  return resource_size(app_num, resource_id);
}

DEFINE_SYSCALL(size_t, sys_resource_load_range, ResAppNum app_num, uint32_t id,
               uint32_t start_offset, uint8_t *data, size_t num_bytes) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (pebble_task_get_current() == PebbleTask_Worker) {
      // Not allowed from workers
      syscall_failed();
    }

    prv_assert_app_num_accessible(app_num);
    syscall_assert_userspace_buffer(data, num_bytes);
  }

  return resource_load_byte_range_system(app_num, id, start_offset, data, num_bytes);
}

DEFINE_SYSCALL(bool, sys_resource_bytes_are_readonly, void *ptr) {
  return resource_bytes_are_readonly(ptr);
}

DEFINE_SYSCALL(const uint8_t *, sys_resource_read_only_bytes, ResAppNum app_num,
               uint32_t resource_id, size_t *num_bytes_out) {
  bool caller_is_privileged = true;
  if (PRIVILEGE_WAS_ELEVATED) {
    caller_is_privileged = false;
    if (pebble_task_get_current() == PebbleTask_Worker) {
      // Not allowed from workers
      syscall_failed();
    }

    prv_assert_app_num_accessible(app_num);

    // num_bytes_out is optional, so it's perfectly safe for an app to pass in NULL here.
    if (num_bytes_out) {
      syscall_assert_userspace_buffer(num_bytes_out, sizeof(*num_bytes_out));
    }
  }

  return resource_get_readonly_bytes(app_num, resource_id, num_bytes_out, caller_is_privileged);
}

DEFINE_SYSCALL(bool, sys_resource_is_valid, ResAppNum app_num, uint32_t resource_id) {
  if (PRIVILEGE_WAS_ELEVATED) {
    prv_assert_app_num_accessible(app_num);
  }

  return resource_is_valid(app_num, resource_id);
}

DEFINE_SYSCALL(uint32_t, sys_resource_get_and_cache, ResAppNum app_num, uint32_t resource_id) {
  if (PRIVILEGE_WAS_ELEVATED && app_num != SYSTEM_APP) {
    syscall_failed();
  }

  return resource_get_and_cache(app_num, resource_id);
}
