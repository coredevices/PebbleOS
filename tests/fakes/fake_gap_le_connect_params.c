/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_gap_le_connect_params.h"

#include "pbl/services/new_timer/new_timer.h"

static ResponseTimeState s_last_requested_desired_state;

void gap_le_connect_params_request(GAPLEConnection *connection,
                                   ResponseTimeState desired_state) {
  s_last_requested_desired_state = desired_state;
}

void gap_le_connect_params_setup_connection(GAPLEConnection *connection, TimerID timer) {
}

void gap_le_connect_params_cleanup_by_connection(GAPLEConnection *connection) {
}

static ResponseTimeState s_actual_state;
ResponseTimeState gap_le_connect_params_get_actual_state(GAPLEConnection *connection) {
  return s_actual_state;
}

void fake_gap_le_connect_params_init(void) {
  s_last_requested_desired_state = ResponseTimeInvalid;
  s_actual_state = ResponseTimeInvalid;
}

ResponseTimeState fake_gap_le_connect_params_get_last_requested(void) {
  return s_last_requested_desired_state;
}

void fake_gap_le_connect_params_reset_last_requested(void) {
  s_last_requested_desired_state = ResponseTimeInvalid;
}

void fake_gap_le_connect_params_set_actual_state(ResponseTimeState actual_state) {
  s_actual_state = actual_state;
}
