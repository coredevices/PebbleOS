/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/vibes/vibe_score_info.h"

#include "clar.h"
#include "resource/resource_ids.auto.h"
// stub
#include "stubs_logging.h"
#include "stubs_passert.h"

#include <string.h>

//unit test code
void test_vibe_score_info__initialize(void) {
}

void test_vibe_score_info__cleanup(void) {
}

void test_vibe_score_info__get_resource_id_returns_correct_resource(void) {
  cl_assert_equal_i(vibe_score_info_get_resource_id(VibeScoreId_Pulse),
                    RESOURCE_ID_VIBE_SCORE_PULSE);
}

void test_vibe_score_info__get_resource_id_returns_invalid_for_invalid_score(void) {
  cl_assert_equal_i(vibe_score_info_get_resource_id(VibeScoreId_Invalid),
                    RESOURCE_ID_INVALID);
}

void test_vibe_score_info__get_name_returns_correct_name(void) {
  cl_assert(strcmp(vibe_score_info_get_name(VibeScoreId_Reveille), "Reveille") == 0);
}

void test_vibe_score_info__get_name_returns_empty_string_for_invalid_score(void) {
  cl_assert(strcmp(vibe_score_info_get_name(VibeScoreId_Invalid), "") == 0);
}

static void prv_test_cycle_next(VibeClient client, const VibeScoreId scores[], size_t scores_size,
                                VibeScoreId starting_score, int curr_score_index) {
  VibeScoreId curr_score = starting_score;
  while ((curr_score = vibe_score_info_cycle_next(client, curr_score),
          curr_score != starting_score)) {
    curr_score_index = (curr_score_index + 1) % scores_size;
    cl_assert_equal_i(scores[curr_score_index], curr_score);
  }
}

void test_vibe_score_info__cycle_next_notifications(void) {
  const VibeScoreId notification_scores[] = {
    VibeScoreId_Disabled,
    VibeScoreId_StandardShortPulseLow,
    VibeScoreId_StandardShortPulseHigh,
    VibeScoreId_Pulse,
    VibeScoreId_NudgeNudge,
    VibeScoreId_Jackhammer,
    VibeScoreId_Mario,
  };

  prv_test_cycle_next(VibeClient_Notifications, notification_scores, 7, VibeScoreId_Pulse, 3);
}

void test_vibe_score_info__cycle_next_calls(void) {
  const VibeScoreId call_scores[] = {
    VibeScoreId_Disabled,
    VibeScoreId_StandardLongPulseLow,
    VibeScoreId_StandardLongPulseHigh,
    VibeScoreId_Pulse,
    VibeScoreId_NudgeNudge,
    VibeScoreId_Jackhammer,
    VibeScoreId_Mario,
  };

  prv_test_cycle_next(VibeClient_PhoneCalls, call_scores, 7, VibeScoreId_Jackhammer, 5);
}

void test_vibe_score_info__cycle_next_alarms(void) {
  const VibeScoreId alarm_scores[] = {
    VibeScoreId_StandardLongPulseLow,
    VibeScoreId_StandardLongPulseHigh,
    VibeScoreId_Pulse,
    VibeScoreId_NudgeNudge,
    VibeScoreId_Jackhammer,
    VibeScoreId_Reveille,
    VibeScoreId_Mario,
  };

  prv_test_cycle_next(VibeClient_Alarms, alarm_scores, 7, VibeScoreId_NudgeNudge, 3);
}

void test_vibe_score_info__is_valid_true_for_valid_score(void) {
  cl_assert(vibe_score_info_is_valid(VibeScoreId_Pulse));
}

void test_vibe_score_info__is_valid_false_for_invalid_source_id(void) {
  cl_assert(!vibe_score_info_is_valid(VibeScoreId_Invalid));
}
