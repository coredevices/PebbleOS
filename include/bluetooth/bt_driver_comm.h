/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

typedef struct CommSession CommSession;

//! Figures out the optimal thread to execute `bt_driver_run_send_next_job` on
//! and schedules a job to do so
bool bt_driver_comm_schedule_send_next_job(CommSession *session);

//! @return The PebbleTask that is used with bt_driver_comm_schedule_send_next_job() to perform
//! the sending of pending data.
bool bt_driver_comm_is_current_task_send_next_task(void);

extern void bt_driver_run_send_next_job(CommSession *session, bool is_callback);
