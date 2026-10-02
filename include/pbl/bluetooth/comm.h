/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup bluetooth_comm Comm session scheduling
 * @ingroup bluetooth
 * @brief Pick the task that sends a comm session's pending data.
 * @{
 */

/** @brief Pebble Protocol session (see @c services/comm_session). */
typedef struct CommSession CommSession;

/**
 * @brief Schedule pbl_bt_run_send_next_job() for a session.
 *
 * The NimBLE backend runs it on KernelMain.
 *
 * @param session The session with pending data.
 * @return true if the job was scheduled.
 */
bool pbl_bt_comm_schedule_send_next_job(CommSession *session);

/**
 * @brief Check whether the current task is the one pbl_bt_comm_schedule_send_next_job()
 * schedules on.
 *
 * @return true if called from that task.
 */
bool pbl_bt_comm_is_current_task_send_next_task(void);

/**
 * @brief Send a session's pending data.
 *
 * Implemented by the firmware, invoked by the job scheduled with
 * pbl_bt_comm_schedule_send_next_job().
 *
 * @param session The session.
 * @param is_callback true when invoked from the scheduled job.
 */
extern void pbl_bt_run_send_next_job(CommSession *session, bool is_callback);

/** @} */
