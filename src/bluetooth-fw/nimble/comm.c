/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/bt_driver_comm.h>
#include <kernel/event_loop.h>

static void prv_send_job(void *data) {
  CommSession *session = (CommSession *)data;
  bt_driver_run_send_next_job(session, true);
}

bool bt_driver_comm_schedule_send_next_job(CommSession *session) {
  launcher_task_add_callback(prv_send_job, session);
  return true;  // we croak if a task cannot be scheduled on KernelMain
}

bool bt_driver_comm_is_current_task_send_next_task(void) { return launcher_task_is_current_task(); }
