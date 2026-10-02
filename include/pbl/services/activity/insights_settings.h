/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "activity.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_activity_insights_settings Insights settings
 * @ingroup services_activity
 * @brief Tunable parameters of the activity insights.
 *
 * Settings are stored per insight, keyed by name, in the @c insights settings file. Built-in
 * defaults are used for insights missing from the file.
 *
 * @code{.c}
 * ActivityInsightSettings settings;
 *
 * if (activity_insights_settings_read(ACTIVITY_INSIGHTS_SETTINGS_SLEEP_REWARD, &settings) &&
 *     settings.enabled) {
 *   uint16_t delay_s = settings.reward.sleep.trigger_after_wakeup_seconds;
 * }
 * @endcode
 * @{
 */

/** @brief Key of the sleep reward settings. */
#define ACTIVITY_INSIGHTS_SETTINGS_SLEEP_REWARD "sleep_reward"
/** @brief Key of the sleep summary settings. */
#define ACTIVITY_INSIGHTS_SETTINGS_SLEEP_SUMMARY "sleep_summary"
/** @brief Key of the activity reward settings. */
#define ACTIVITY_INSIGHTS_SETTINGS_ACTIVITY_REWARD "activity_reward"
/** @brief Key of the activity summary settings. */
#define ACTIVITY_INSIGHTS_SETTINGS_ACTIVITY_SUMMARY "activity_summary"
/** @brief Key of the activity session settings. */
#define ACTIVITY_INSIGHTS_SETTINGS_ACTIVITY_SESSION "activity_session"

/**
 * @brief Reward insight settings.
 *
 * Day counts are in addition to today.
 */
typedef struct PBL_PACKED ActivityRewardSettings {
  /** Days of history required. */
  uint8_t min_days_data;
  /** Consecutive days of history required. */
  uint8_t continuous_min_days_data;
  /** Days that must be above target, in addition to today. */
  uint8_t target_qualifying_days;

  /** Percentage of the median that qualifying days must reach. */
  uint16_t target_percent_of_median;
  /** Minimum interval between two notifications of this insight, in seconds. */
  uint32_t notif_min_interval_seconds;

  /** Insight specific settings. */
  union {
    /** Sleep reward settings. */
    struct PBL_PACKED {
      /** Delay after waking up before showing the reward, in seconds. */
      uint16_t trigger_after_wakeup_seconds;
    } sleep;

    /** Activity reward settings. */
    struct PBL_PACKED {
      /** Minutes the user must currently have been active before showing the reward. */
      uint8_t trigger_active_minutes;
      /** Steps per minute required for an active minute. */
      uint8_t trigger_steps_per_minute;
    } activity;
  };
} ActivityRewardSettings;

/**
 * @brief Summary pin settings.
 *
 * Thresholds are percentages relative to 100% of the average: 105% is 5, 93% is -7.
 */
typedef struct PBL_PACKED ActivitySummarySettings {
  /** Values above this are above average. */
  int8_t above_avg_threshold;
  /** Values below this are below average. */
  int8_t below_avg_threshold;
  /** Values below this are a fail. */
  int8_t fail_threshold;

  /** Insight specific settings. */
  union {
    /** Activity summary settings. */
    struct PBL_PACKED {
      /** Minute of the day at which the pin is added. */
      uint16_t trigger_minute;
      /** Step change that causes the pin to be updated. */
      uint16_t update_threshold_steps;
      /** Maximum time without updating the pin, in seconds. */
      uint32_t update_max_interval_seconds;
      /** Whether to show a notification. */
      bool show_notification;
      /** Do not show a negative summary above this many steps. */
      uint16_t max_fail_steps;
    } activity;

    /** Sleep summary settings. */
    struct PBL_PACKED {
      /** Do not show a negative summary above this many minutes of sleep. */
      uint16_t max_fail_minutes;
      /** Delay after waking up before the notification, in seconds. */
      uint16_t trigger_notif_seconds;
      /** Minimum steps per minute to trigger the notification. */
      uint16_t trigger_notif_activity;
      /** Minimum active minutes to trigger the notification. */
      uint8_t trigger_notif_active_minutes;
    } sleep;
  };
} ActivitySummarySettings;

/** @brief Activity session insight settings. */
typedef struct PBL_PACKED ActivitySessionSettings {
  /** Whether to show a notification. */
  bool show_notification;

  /** Session type specific settings. */
  union {
    /** Walk and run settings. */
    struct PBL_PACKED {
      /** Minimum session length to get an insight, in minutes. */
      uint16_t trigger_elapsed_minutes;
      /** Delay after the end of the session before notifying, in minutes. */
      uint16_t trigger_cooldown_minutes;
    } activity;
  };
} ActivitySessionSettings;

/** @brief Settings of one insight. */
typedef struct PBL_PACKED ActivityInsightSettings {
  // Common parameters
  /** Struct version, must be first. Records with another version are ignored. */
  uint8_t version;

  /** Insight enabled. */
  bool enabled;
  /** Unused. */
  uint8_t unused;

  /** Insight specific settings. */
  union {
    /** Reward settings. */
    ActivityRewardSettings reward;
    /** Summary pin settings. */
    ActivitySummarySettings summary;
    /** Activity session settings. */
    ActivitySessionSettings session;
  };
} ActivityInsightSettings;

/**
 * @brief Read the settings of an insight.
 *
 * Falls back to the built-in defaults when the file has no valid record for the insight.
 *
 * @param insight_name Insight key, e.g. @ref ACTIVITY_INSIGHTS_SETTINGS_SLEEP_REWARD.
 * @param[out] settings_out Settings; zeroed on failure.
 * @return true if settings were found, false otherwise.
 */
bool activity_insights_settings_read(const char *insight_name,
                                     ActivityInsightSettings *settings_out);

/**
 * @brief Write the settings of an insight (testing).
 *
 * @param insight_name Insight key.
 * @param settings Settings.
 * @return true if saved.
 */
bool activity_insights_settings_write(const char *insight_name, ActivityInsightSettings *settings);

/**
 * @brief Get the version of the insights settings file contents.
 *
 * Separate from @ref ActivityInsightSettings::version.
 *
 * @return Version, 0 by default.
 */
uint16_t activity_insights_settings_get_version(void);

/** @brief Initialize the insights settings file. */
void activity_insights_settings_init(void);

/**
 * @brief Watch the insights settings file.
 *
 * @param callback Called when the file is closed after modifications, or deleted.
 * @return Handle for activity_insights_settings_unwatch(), 0 if the activity service is not
 * initialized.
 */
PFSCallbackHandle activity_insights_settings_watch(PFSFileChangedCallback callback);

/**
 * @brief Stop watching the insights settings file.
 *
 * @param cb_handle Handle returned by activity_insights_settings_watch().
 */
void activity_insights_settings_unwatch(PFSCallbackHandle cb_handle);

/** @} */
