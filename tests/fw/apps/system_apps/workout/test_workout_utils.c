/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/workout/workout_utils.h"

#include "clar.h"

#include "services/normal/activity/activity.h"

// ---------------------------------------------------------------------------------------
#include "stubs_attribute.h"
#include "stubs_i18n.h"
#include "stubs_notifications.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_rtc.h"
#include "stubs_timeline_item.h"

// Stubs
///////////////////////////////////////////////////////////

bool workout_service_is_workout_type_supported(ActivitySessionType type) {
  return true;
}

// Fakes
///////////////////////////////////////////////////////////

static ActivitySession s_sessions[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
static uint32_t s_num_sessions = 0;

static void prv_add_session(ActivitySession *session) {
  memcpy(&s_sessions[s_num_sessions++], session, sizeof(ActivitySession));
}

// ---------------------------------------------------------------------------------------
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  memcpy(sessions, s_sessions, s_num_sessions * sizeof(ActivitySession));
  *session_entries = s_num_sessions;
  return true;
}

// ---------------------------------------------------------------------------------------
void test_workout_utils__initialize(void) {
  s_num_sessions = 0;
}

void test_workout_utils__cleanup(void) {
}

// ---------------------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------------------
void test_workout_utils__find_ongoing_activity_session(void) {
  bool found_session;

  // Check if it can handle NULL session
  found_session = workout_utils_find_ongoing_activity_session(NULL);
  cl_assert_equal_b(found_session, false);

  // Make sure there are no sessions
  cl_assert_equal_i(s_num_sessions, 0);

  // Add a non-ongoing walk session
  prv_add_session(&(ActivitySession){
    .type = ActivitySessionType_Walk,
    .ongoing = false,
  });

  // Make sure the session was added
  cl_assert_equal_i(s_num_sessions, 1);

  // Find the session we just added
  ActivitySession walk_session = {};
  found_session = workout_utils_find_ongoing_activity_session(&walk_session);

  // Made sure non-ongoing sessions are not returned
  cl_assert_equal_b(found_session, false);

  // Add an ongoing run session
  prv_add_session(&(ActivitySession){
    .type = ActivitySessionType_Run,
    .ongoing = true,
  });

  // Make sure the session was added
  cl_assert_equal_i(s_num_sessions, 2);

  // Find the session we just added
  ActivitySession run_session = {};
  found_session = workout_utils_find_ongoing_activity_session(&run_session);

  // Make sure the function returned true and the returned session is of type run
  cl_assert_equal_b(found_session, true);
  cl_assert_equal_i(run_session.type, ActivitySessionType_Run);
}
