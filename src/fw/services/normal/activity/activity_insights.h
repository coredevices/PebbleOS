/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "activity_private.h"
#include "util/time/time.h"

#include <stdint.h>

typedef enum PercentTier {
  PercentTier_AboveAverage = 0,
  PercentTier_OnAverage,
  PercentTier_BelowAverage,
  PercentTier_Fail,
  PercentTierCount
} PercentTier;

// Insight types (for analytics)
typedef enum ActivityInsightType {
  ActivityInsightType_Unknown = 0,
  ActivityInsightType_SleepReward,
  ActivityInsightType_ActivityReward,
  ActivityInsightType_SleepSummary,
  ActivityInsightType_ActivitySummary,
  ActivityInsightType_Day1,
  ActivityInsightType_Day4,
  ActivityInsightType_Day10,
  ActivityInsightType_ActivitySessionSleep,
  ActivityInsightType_ActivitySessionNap,
  ActivityInsightType_ActivitySessionWalk,
  ActivityInsightType_ActivitySessionRun,
  ActivityInsightType_ActivitySessionOpen,
} ActivityInsightType;

// Insight response types (for analytics)
typedef enum ActivityInsightResponseType {
  ActivityInsightResponseTypePositive = 0,
  ActivityInsightResponseTypeNeutral,
  ActivityInsightResponseTypeNegative,
  ActivityInsightResponseTypeClassified,
  ActivityInsightResponseTypeMisclassified,
} ActivityInsightResponseType;

typedef enum ActivationDelayInsightType {
  // New vals must be added on the end. These are used in a prefs bitfield
  ActivationDelayInsightType_Day1,
  ActivationDelayInsightType_Day4,
  ActivationDelayInsightType_Day10,
  ActivationDelayInsightTypeCount,
} ActivationDelayInsightType;

// Various stats for metrics that are used to determine when it's ok to trigger an insight
typedef struct ActivityInsightMetricHistoryStats {
  uint8_t total_days;
  uint8_t consecutive_days;
  ActivityScalarStore median;
  ActivityScalarStore mean;
  ActivityMetric metric;
} ActivityInsightMetricHistoryStats;

//! Called at midnight rollover to recalculate medians/totals for metric history
//! IMPORTANT: This call is not thread safe and must only be called when activity.c is holding
//! s_activity_state.mutex
void activity_insights_recalculate_stats(void);

//! Init activity insights
//! IMPORTANT: This call is not thread safe and should only be called from activity_init (since it
//! is called during boot when no other task might use an activity service call)
//! @param[in] now_utc Current time
void activity_insights_init(time_t now_utc);

//! Called by prv_minute_system_task_cb whenever it updates sleep metrics
//! IMPORTANT: This call is not thread safe and must only be called when activity.c is holding
//! s_activity_state.mutex
//! @param[in] now_utc Current time
void activity_insights_process_sleep_data(time_t now_utc);

//! Called once per minute by prv_minute_system_task_cb to check step insights
//! IMPORTANT: This call is not thread safe and must only be called when activity.c is holding
//! s_activity_state.mutex
//! @param[in] now_utc Current time
void activity_insights_process_minute_data(time_t now_utc);

void activity_insights_push_activity_session_notification(time_t notif_time,
                                                          ActivitySession *session,
                                                          int32_t avg_hr,
                                                          int32_t *hr_zone_time_s);

//! Used by test apps: Pushes the 3 variants of each summary pin to the timeline and a notification
//! for the last variant of each
void activity_insights_test_push_summary_pins(void);

//! Used by test apps: Pushes the 2 rewards to the watch
void activity_insights_test_push_rewards(void);

//! Used by test apps: Pushes the day 1, 4 and 10 insights
void activity_insights_test_push_day_insights(void);

//! Used by test apps: Pushes a run and a walk notification
void activity_insights_test_push_walk_run_sessions(void);

//! Used by test apps: Pushes a nap pin and notification
void activity_insights_test_push_nap_session(void);
