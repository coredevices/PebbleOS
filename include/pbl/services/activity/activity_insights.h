/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "activity_private.h"

#include <stdint.h>
#include <time.h>

/**
 * @defgroup services_activity_activity_insights Activity insights
 * @ingroup services_activity
 * @brief Sleep and activity rewards, summary pins and session notifications.
 *
 * Insights compare today's sleep and steps against the user's history and push timeline pins
 * and notifications. Their parameters come from the insights settings (see
 * @ref services_activity_insights_settings).
 * @{
 */

/** @brief How a value compares to the user's average. */
typedef enum PercentTier {
  /** Above the average threshold. */
  PercentTier_AboveAverage = 0,
  /** Between the below and above average thresholds. */
  PercentTier_OnAverage,
  /** Below the average threshold. */
  PercentTier_BelowAverage,
  /** Below the fail threshold. */
  PercentTier_Fail,
  /** Number of tiers. */
  PercentTierCount
} PercentTier;

/** @brief Insight types, reported to analytics. */
typedef enum ActivityInsightType {
  /** Unknown. */
  ActivityInsightType_Unknown = 0,
  /** Sleep reward. */
  ActivityInsightType_SleepReward,
  /** Activity reward. */
  ActivityInsightType_ActivityReward,
  /** Sleep summary pin. */
  ActivityInsightType_SleepSummary,
  /** Activity summary pin. */
  ActivityInsightType_ActivitySummary,
  /** Day 1 after activation. */
  ActivityInsightType_Day1,
  /** Day 4 after activation. */
  ActivityInsightType_Day4,
  /** Day 10 after activation. */
  ActivityInsightType_Day10,
  /** Sleep session. */
  ActivityInsightType_ActivitySessionSleep,
  /** Nap session. */
  ActivityInsightType_ActivitySessionNap,
  /** Walk session. */
  ActivityInsightType_ActivitySessionWalk,
  /** Run session. */
  ActivityInsightType_ActivitySessionRun,
  /** Open workout session. */
  ActivityInsightType_ActivitySessionOpen,
} ActivityInsightType;

/** @brief User responses to an insight, reported to analytics. */
typedef enum ActivityInsightResponseType {
  /** Positive feedback. */
  ActivityInsightResponseTypePositive = 0,
  /** Neutral feedback. */
  ActivityInsightResponseTypeNeutral,
  /** Negative feedback. */
  ActivityInsightResponseTypeNegative,
  /** Session correctly classified. */
  ActivityInsightResponseTypeClassified,
  /** Session misclassified. */
  ActivityInsightResponseTypeMisclassified,
} ActivityInsightResponseType;

/**
 * @brief Insights sent a number of days after activation.
 *
 * Used as bit positions in a prefs bitfield: new values go at the end.
 */
typedef enum ActivationDelayInsightType {
  /** Day 1. */
  ActivationDelayInsightType_Day1,
  /** Day 4. */
  ActivationDelayInsightType_Day4,
  /** Day 10. */
  ActivationDelayInsightType_Day10,
  /** Number of activation delay insights. */
  ActivationDelayInsightTypeCount,
} ActivationDelayInsightType;

/**
 * @brief Statistics of a metric's history, used to decide whether an insight can trigger.
 *
 * Computed over past days only, ignoring days with no data.
 */
typedef struct ActivityInsightMetricHistoryStats {
  /** Days with data. */
  uint8_t total_days;
  /** Consecutive days with data, starting yesterday. */
  uint8_t consecutive_days;
  /** Median value. */
  ActivityScalarStore median;
  /** Mean value. */
  ActivityScalarStore mean;
  /** Metric. */
  ActivityMetric metric;
} ActivityInsightMetricHistoryStats;

/**
 * @brief Recalculate the metric history statistics.
 *
 * Called at midnight rollover. Not thread safe: the caller must hold the activity state mutex.
 */
void activity_insights_recalculate_stats(void);

/**
 * @brief Initialize activity insights.
 *
 * Not thread safe: only called from activity_init(), during boot.
 *
 * @param now_utc Current UTC time.
 */
void activity_insights_init(time_t now_utc);

/**
 * @brief Process updated sleep metrics.
 *
 * Called by the minute handler when it updates the sleep metrics. Not thread safe: the caller
 * must hold the activity state mutex.
 *
 * @param now_utc Current UTC time.
 */
void activity_insights_process_sleep_data(time_t now_utc);

/**
 * @brief Check the step insights.
 *
 * Called once per minute by the minute handler. Not thread safe: the caller must hold the
 * activity state mutex.
 *
 * @param now_utc Current UTC time.
 */
void activity_insights_process_minute_data(time_t now_utc);

/**
 * @brief Push the notification summarizing a walk, run or open workout session.
 *
 * Does nothing for sessions shorter than a minute.
 *
 * @param notif_time Notification time, UTC.
 * @param session Session.
 * @param avg_hr Average heart rate, in beats per minute, 0 if unknown.
 * @param hr_zone_time_s Seconds spent in each @ref HRZone (@ref HRZoneCount entries), or NULL.
 */
void activity_insights_push_activity_session_notification(time_t notif_time,
                                                          ActivitySession *session, int32_t avg_hr,
                                                          int32_t *hr_zone_time_s);

/**
 * @brief Push the 3 variants of each summary pin, with a notification for the last one (test
 * apps).
 */
void activity_insights_test_push_summary_pins(void);

/** @brief Push the sleep and activity rewards (test apps). */
void activity_insights_test_push_rewards(void);

/** @brief Push a run and a walk notification (test apps). */
void activity_insights_test_push_walk_run_sessions(void);

/** @brief Push a nap pin and notification (test apps). */
void activity_insights_test_push_nap_session(void);

/** @} */
