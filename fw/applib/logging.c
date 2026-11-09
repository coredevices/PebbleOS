/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <kernel/kernel_applib_state.h>
#include <kernel/pebble_tasks.h>
#include <logging/logging_private.h>
#include <process_state/app_state/app_state.h>
#include <process_state/worker_state/worker_state.h>
#include <syscall/syscall.h>

// Apps and workers log from unprivileged code: they format in their own buffers and reach the
// backends through syscalls.

// Return a pointer to the LogState to use. The LogState contains the buffers for formatting the log
// message. There are two possible LogState instances: one for the app task and one for all other
// (privileged) tasks (which is guarded by a mutex). Returns NULL if a logging operation is already
// in progress
LogState *pbl_log__state_get(void) {
  LogState *log_state = nullptr;

  PebbleTask task = pebble_task_get_current();
  if (task == PebbleTask_App) {
    log_state = app_state_get_log_state();
  } else if (task == PebbleTask_Worker) {
    log_state = worker_state_get_log_state();
  }

  if (log_state) {
    if (log_state->in_progress) {
      return nullptr;
    }
    log_state->in_progress = true;
    return log_state;
  } else {
    return kernel_applib_get_log_state();
  }
}

void pbl_log__state_release(LogState *state) {
  PebbleTask task = pebble_task_get_current();
  if (task == PebbleTask_App || task == PebbleTask_Worker) {
    state->in_progress = false;
  } else {
    kernel_applib_release_log_state(state);
  }
}

void pbl_log__submit(LogBinaryMessage *log_message, uint32_t flags) {
  sys_pbl_log(log_message, flags);
}

uint32_t pbl_log__time(void) {
  return (uint32_t)sys_get_time();
}

uint32_t pbl_log__stack_free(void) {
  return sys_stack_free_bytes();
}
