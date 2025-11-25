/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SmartstrapStateUnsubscribed,
  SmartstrapStateReadReady,
  SmartstrapStateNotifyInProgress,
  SmartstrapStateReadDisabled,
  SmartstrapStateReadInProgress,
  SmartstrapStateReadComplete
} SmartstrapState;


SmartstrapState smartstrap_fsm_state_get(void);
void smartstrap_fsm_state_reset(void);
bool smartstrap_fsm_state_test_and_set(SmartstrapState expected_state, SmartstrapState next_state);
void smartstrap_fsm_state_set(SmartstrapState next_state);
void smartstrap_state_lock(void);
void smartstrap_state_unlock(void);
void smartstrap_state_assert_locked_by_current_task(void);
bool sys_smartstrap_is_service_connected(uint16_t service_id);
