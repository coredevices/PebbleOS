/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/bluetooth/comm.h"
#include "kernel/event_loop.h"

#include <stdbool.h>

static void prv_send_job(void *data) {
  CommSession *session = (CommSession *)data;
  pbl_bt_run_send_next_job(session, true);
}

bool pbl_bt_comm_schedule_send_next_job(CommSession *session) {
  launcher_task_add_callback(prv_send_job, session);
  return true; // we croak if a task cannot be scheduled on KernelMain
}

bool pbl_bt_comm_is_current_task_send_next_task(void) {
  return launcher_task_is_current_task();
}
