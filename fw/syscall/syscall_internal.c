/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall_internal.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/privilege.h>

#include <applib/app_logging.h>
#include <kernel/pebble_tasks.h>
#include <logging/logging_private.h>
#include <process_management/process_manager.h>
#include <syscall/syscall.h>
#include <syscall/syscall_define.h>

[[noreturn]] void syscall_failed(void) {
  register uint32_t lr __asm("lr");
  uint32_t saved_lr = lr;

  PBL_ASSERT(mcu_state_is_privileged(), "Insufficient Privileges!");

  PBL_LOG_WRN("Bad syscall!");

  sys_app_fault(saved_lr);

  // sys_die is no return, but it's a syscall so I don't want to mark it with that attribute
  while (1) {
  }
}

static bool prv_is_userspace_buffer(const void *buf, size_t num_bytes) {
  return process_manager_is_range_in_region(pebble_task_get_current(), buf, num_bytes,
                                            (void *)arch_syscall_caller_sp());
}

void syscall_assert_userspace_buffer(const void *buf, size_t num_bytes) {
  if (prv_is_userspace_buffer(buf, num_bytes)) {
    return;
  }

  APP_LOG(APP_LOG_LEVEL_ERROR, "syscall failure! %p..%p is not in app space.", buf,
          (char *)buf + num_bytes);
  PBL_LOG_ERR("syscall failure! %p..%p is not in app space.", buf, (char *)buf + num_bytes);
  syscall_failed();
}

int syscall_copy_from_user(void *dst, const void *src, size_t num_bytes) {
  if (!prv_is_userspace_buffer(src, num_bytes)) {
    return -EFAULT;
  }

  memcpy(dst, src, num_bytes);
  return 0;
}

int syscall_copy_to_user(void *dst, const void *src, size_t num_bytes) {
  if (!prv_is_userspace_buffer(dst, num_bytes)) {
    return -EFAULT;
  }

  memcpy(dst, src, num_bytes);
  return 0;
}

void syscall_assert_userspace_log_message(const LogBinaryMessage *log_message) {
  syscall_assert_userspace_buffer(log_message, sizeof(*log_message));

  const size_t message_length = log_message->message_length;
  syscall_assert_userspace_buffer(log_message, sizeof(*log_message) + message_length);

  if (strnlen(log_message->filename, sizeof(log_message->filename)) ==
      sizeof(log_message->filename)) {
    syscall_failed();
  }

  if (strnlen(log_message->message, message_length) == message_length) {
    syscall_assert_userspace_buffer(log_message, sizeof(*log_message) + message_length + 1);
    if (log_message->message[message_length] != '\0') {
      syscall_failed();
    }
  }
}

int syscall_user_strnlen(const char *str, size_t max) {
  size_t window = process_manager_region_bytes_from(pebble_task_get_current(), str,
                                                    (void *)arch_syscall_caller_sp());
  if (max < window) {
    window = max + 1;
  }
  const size_t len = strnlen(str, window);
  if (len == window) {
    return -EFAULT;
  }
  return (int)len;
}

void syscall_copy_out(struct syscall_copy_out *out) {
  if (out->dst != nullptr && syscall_copy_to_user(out->dst, out->src, out->num_bytes) < 0) {
    syscall_failed();
  }
}
