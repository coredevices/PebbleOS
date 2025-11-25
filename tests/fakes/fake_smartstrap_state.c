/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar_asserts.h"

#include "fake_smartstrap_state.h"

static bool s_locked = false;
static SmartstrapState s_fsm_state = SmartstrapStateUnsubscribed;

static void prv_check_fsm_transition(SmartstrapState prev_state, SmartstrapState new_state) {
  if (new_state == SmartstrapStateUnsubscribed) {
  } else if ((prev_state == SmartstrapStateUnsubscribed) &&
             (new_state == SmartstrapStateReadReady)) {
  } else if ((prev_state == SmartstrapStateReadReady) &&
             (new_state == SmartstrapStateNotifyInProgress)) {
  } else if ((prev_state == SmartstrapStateReadReady) &&
             (new_state == SmartstrapStateReadDisabled)) {
  } else if ((prev_state == SmartstrapStateNotifyInProgress) &&
             (new_state == SmartstrapStateReadComplete)) {
  } else if ((prev_state == SmartstrapStateReadDisabled) &&
             (new_state == SmartstrapStateReadInProgress)) {
  } else if ((prev_state == SmartstrapStateReadDisabled) &&
             (new_state == SmartstrapStateReadReady)) {
  } else if ((prev_state == SmartstrapStateReadInProgress) &&
             (new_state == SmartstrapStateReadComplete)) {
  } else if ((prev_state == SmartstrapStateReadComplete) &&
             (new_state == SmartstrapStateReadReady)) {
  } else {
    // all other transitions are invalid
    cl_assert(false);
  }
}

SmartstrapState smartstrap_fsm_state_get(void) {
  return s_fsm_state;
}

void smartstrap_fsm_state_reset(void) {
  s_fsm_state = SmartstrapStateReadReady;
}

bool smartstrap_fsm_state_test_and_set(SmartstrapState expected_state, SmartstrapState next_state) {
  if (s_fsm_state != expected_state) {
    return false;
  }
  prv_check_fsm_transition(s_fsm_state, next_state);
  s_fsm_state = next_state;
  return true;
}

void smartstrap_fsm_state_set(SmartstrapState next_state) {
  prv_check_fsm_transition(s_fsm_state, next_state);
  s_fsm_state = next_state;
}

void smartstrap_state_lock(void) {
  cl_assert(!s_locked);
  s_locked = true;
}

void smartstrap_state_unlock(void) {
  cl_assert(s_locked);
  s_locked = false;
}

void smartstrap_state_assert_locked_by_current_task(void) {
  cl_assert(s_locked);
}

bool sys_smartstrap_is_service_connected(uint16_t service_id) {
  return true;
}
