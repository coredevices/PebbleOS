/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "applib/accel_service_private.h"
#include "applib/health_service.h"
#include "pbl/kernel/compiler.h"
#include "pbl/util/time.h"
#include "pbl/util/units.h"

/**
 * @defgroup services_activity Activity
 * @ingroup services
 * @brief Step counting, sleep tracking, activity sessions and health metrics.
 *
 * The activity service samples the accelerometer at 25 Hz and feeds the samples to the activity
 * algorithm (see @ref services_activity_kraepelin) on KernelBG. The algorithm counts steps over
 * 5 second epochs and, once a minute, produces a minute record (steps, VMC, orientation, light,
 * heart rate, SpO2) that is stored in a minute file and sent to the phone through data logging.
 * From the minute data it detects sleep, restful sleep, walks and runs, reported as
 * @ref ActivitySession entries.
 *
 * Daily metrics are reset at midnight and the last @ref ACTIVITY_HISTORY_DAYS days are kept in
 * the @c activity settings file. Units used across the service:
 * - steps: count;
 * - distance: meters (millimeters internally);
 * - calories: kcal (calories, 1/1000 kcal, internally);
 * - durations: seconds through activity_get_metric(), minutes in sessions and storage;
 * - heart rate: beats per minute;
 * - user height: millimeters, weight: decagrams (10 g).
 *
 * Today's step count and the previous days:
 *
 * @code{.c}
 * int32_t steps[7];
 *
 * if (activity_get_metric(ActivityMetricStepCount, ARRAY_LENGTH(steps), steps)) {
 *   // steps[0] is today, steps[1] yesterday, ...; -1 where no data is available
 * }
 *
 * int32_t sleep_s;
 * activity_get_metric(ActivityMetricSleepTotalSeconds, 1, &sleep_s);
 * @endcode
 *
 * Sessions detected today:
 *
 * @code{.c}
 * ActivitySession sessions[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
 * uint32_t count = ARRAY_LENGTH(sessions);
 *
 * if (activity_get_sessions(&count, sessions)) {
 *   for (uint32_t i = 0; i < count; i++) {
 *     if (sessions[i].type == ActivitySessionType_Walk) {
 *       // sessions[i].length_min, sessions[i].step_data.steps, ...
 *     }
 *   }
 * }
 * @endcode
 * @{
 */

/** @brief Number of days of metric history kept, today included. */
#define ACTIVITY_HISTORY_DAYS 30

/**
 * @brief Maximum number of activity sessions cached at a time.
 *
 * A night usually produces 4 or 5 sleep sessions (one container plus restful periods), plus a
 * handful of walks and runs.
 */
#define ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT 32

/** @brief Number of calories in a kilocalorie. */
#define ACTIVITY_CALORIES_PER_KCAL 1000

/** @brief User gender, used by the calorie computations. */
typedef enum {
  /** Female. */
  ActivityGenderFemale = 0,
  /** Male. */
  ActivityGenderMale = 1,
  /** Other; calorie formulas use the midpoint between female and male. */
  ActivityGenderOther = 2
} ActivityGender;

/** @brief User profile and activity preferences, as stored in prefs. */
typedef struct PBL_PACKED ActivitySettings {
  /** Height, in millimeters. */
  int16_t height_mm;
  /** Weight, in decagrams (10 g). */
  int16_t weight_dag;
  /** Activity tracking enabled. */
  bool tracking_enabled;
  /** Activity insights enabled. */
  bool activity_insights_enabled;
  /** Sleep insights enabled. */
  bool sleep_insights_enabled;
  /** Age, in years. */
  int8_t age_years;
  /** @ref ActivityGender value. */
  int8_t gender;
} ActivitySettings;

/** @brief Heart rate thresholds, in beats per minute, as stored in prefs. */
typedef struct PBL_PACKED HeartRatePreferences {
  /** Resting heart rate. */
  uint8_t resting_hr;
  /** Heart rate at or above which the rate is considered elevated. */
  uint8_t elevated_hr;
  /** Maximum heart rate. */
  uint8_t max_hr;
  /** Lowest heart rate of zone 1. */
  uint8_t zone1_threshold;
  /** Lowest heart rate of zone 2. */
  uint8_t zone2_threshold;
  /** Lowest heart rate of zone 3. */
  uint8_t zone3_threshold;
} HeartRatePreferences;

/**
 * @brief Background measurement interval for heart rate and SpO2.
 *
 * Values are persisted: new ones go at the end.
 */
typedef enum {
  /** Every 10 minutes (default). */
  HRMonitoringInterval_10Min = 0,
  /** Every 30 minutes. */
  HRMonitoringInterval_30Min,
  /** Every hour. */
  HRMonitoringInterval_1Hour,
  /** No background measurements. */
  HRMonitoringInterval_Disabled,
  /** Every 5 minutes (heart rate only; SpO2 falls back to 10 minutes). */
  HRMonitoringInterval_5Min,
  /** Number of intervals. */
  HRMonitoringIntervalCount,
} HRMonitoringInterval;

/** @brief Heart rate monitor preferences, as stored in prefs. */
typedef struct PBL_PACKED ActivityHRMSettings {
  /** Heart rate monitoring enabled. */
  bool enabled;
  /** @ref HRMonitoringInterval value. */
  uint8_t measurement_interval;
  /** Continuous heart rate tracking during detected walks and runs. */
  bool activity_tracking_enabled;
} ActivityHRMSettings;

/**
 * @brief Blood oxygen (SpO2) preferences, as stored in prefs.
 *
 * The on/off setting is synced from the phone under its own key
 * (@c PREF_KEY_BLOOD_OXYGEN_PREFERENCES); only the watch-local interval lives here.
 */
typedef struct PBL_PACKED ActivitySpO2Settings {
  /** @ref HRMonitoringInterval value. */
  uint8_t measurement_interval;
} ActivitySpO2Settings;

/** @brief Default user height, in millimeters (5'3.8", CDC average). */
#define ACTIVITY_DEFAULT_HEIGHT_MM 1620
/** @brief Default user weight, in decagrams (166.2 lbs, CDC average). */
#define ACTIVITY_DEFAULT_WEIGHT_DAG 7539
/** @brief Default user gender. */
#define ACTIVITY_DEFAULT_GENDER ActivityGenderFemale
/** @brief Default user age, in years. */
#define ACTIVITY_DEFAULT_AGE_YEARS 30

/** @brief Initializer for the default @ref ActivitySettings. */
#define ACTIVITY_DEFAULT_PREFERENCES           \
  {                                            \
    .tracking_enabled = false,                 \
    .activity_insights_enabled = false,        \
    .sleep_insights_enabled = false,           \
    .age_years = ACTIVITY_DEFAULT_AGE_YEARS,   \
    .gender = ACTIVITY_DEFAULT_GENDER,         \
    .height_mm = ACTIVITY_DEFAULT_HEIGHT_MM,   \
    .weight_dag = ACTIVITY_DEFAULT_WEIGHT_DAG, \
  }

/**
 * @brief Initializer for the default @ref HeartRatePreferences.
 *
 * Zone thresholds are 50%, 70% and 85% of the heart rate reserve.
 */
#define ACTIVITY_HEART_RATE_DEFAULT_PREFERENCES \
  {                                             \
    .resting_hr = 70,                           \
    .elevated_hr = 100,                         \
    .max_hr = 220 - ACTIVITY_DEFAULT_AGE_YEARS, \
    .zone1_threshold = 130 /* 50% of HRR */,    \
    .zone2_threshold = 154 /* 70% of HRR */,    \
    .zone3_threshold = 172 /* 85% of HRR */,    \
  }

/** @brief Initializer for the default @ref ActivityHRMSettings. */
#define ACTIVITY_HRM_DEFAULT_PREFERENCES                \
  {                                                     \
    .enabled = true,                                    \
    .measurement_interval = HRMonitoringInterval_10Min, \
    .activity_tracking_enabled = false,                 \
  }

/** @brief Initializer for the default @ref ActivitySpO2Settings. */
#define ACTIVITY_SPO2_DEFAULT_PREFERENCES               \
  {                                                     \
    .measurement_interval = HRMonitoringInterval_10Min, \
  }

/** @brief Lowest heart rate reading accepted as valid, in beats per minute. */
#define ACTIVITY_DEFAULT_MIN_HR 40
/** @brief Highest heart rate reading accepted as valid, in beats per minute. */
#define ACTIVITY_DEFAULT_MAX_HR 200

/**
 * @brief Metrics returned by activity_get_metric().
 *
 * Unless noted, values are totals for the current day (since local midnight). Only metrics
 * marked as such have history.
 */
typedef enum {
  /** First metric. */
  ActivityMetricFirst = 0,
  /** Steps taken. Has history. */
  ActivityMetricStepCount = ActivityMetricFirst,
  /** Seconds spent in active minutes (40 steps or more). Has history. */
  ActivityMetricActiveSeconds,
  /** Resting kcal burned. Has history. */
  ActivityMetricRestingKCalories,
  /** Active kcal burned. Has history. */
  ActivityMetricActiveKCalories,
  /** Distance walked or run, in meters. Has history. */
  ActivityMetricDistanceMeters,
  /** Seconds of sleep. Has history. */
  ActivityMetricSleepTotalSeconds,
  /** Seconds of restful (deep) sleep. Has history. */
  ActivityMetricSleepRestfulSeconds,
  /** Time the user fell asleep, in seconds after midnight. Has history. */
  ActivityMetricSleepEnterAtSeconds,
  /** Time the user woke up, in seconds after midnight. Has history. */
  ActivityMetricSleepExitAtSeconds,
  /** Current @ref ActivitySleepState value. */
  ActivityMetricSleepState,
  /** Seconds spent so far in the current @ref ActivityMetricSleepState. */
  ActivityMetricSleepStateSeconds,
  /** VMC (vector magnitude counts) of the last processed minute. */
  ActivityMetricLastVMC,

  /** Most recent heart rate reading, in beats per minute. */
  ActivityMetricHeartRateRawBPM,
  /** Quality of the most recent heart rate reading, an @c HRMQuality value. */
  ActivityMetricHeartRateRawQuality,
  /** UTC time of the most recent heart rate reading. */
  ActivityMetricHeartRateRawUpdatedTimeUTC,
  /** Most recent stable heart rate (median over a minute), in beats per minute. */
  ActivityMetricHeartRateFilteredBPM,
  /** UTC time of the most recent stable heart rate. */
  ActivityMetricHeartRateFilteredUpdatedTimeUTC,

  /** Minutes spent in heart rate zone 1 today. */
  ActivityMetricHeartRateZone1Minutes,
  /** Minutes spent in heart rate zone 2 today. */
  ActivityMetricHeartRateZone2Minutes,
  /** Minutes spent in heart rate zone 3 today. */
  ActivityMetricHeartRateZone3Minutes,

  // KEEP THIS AT THE END
  /** Number of metrics. */
  ActivityMetricNumMetrics,
  /** Invalid metric. */
  ActivityMetricInvalid = ActivityMetricNumMetrics,
} ActivityMetric;

/** @brief Activity session types, used in @ref ActivitySession. Values are logged to the phone. */
typedef enum {
  /** No session. */
  ActivitySessionType_None = 0,

  /**
   * Entire sleep session from falling asleep to waking up, containing both light and restful
   * periods.
   */
  ActivitySessionType_Sleep = 1,

  /** Restful period, always inside an @ref ActivitySessionType_Sleep session. */
  ActivitySessionType_RestfulSleep = 2,

  /** Like @ref ActivitySessionType_Sleep, but labeled a nap because of its duration and time. */
  ActivitySessionType_Nap = 3,

  /** Restful period, always inside an @ref ActivitySessionType_Nap session. */
  ActivitySessionType_RestfulNap = 4,

  /** Walk of significant length. */
  ActivitySessionType_Walk = 5,

  /** Run. */
  ActivitySessionType_Run = 6,

  /** Open (generic) workout. */
  ActivitySessionType_Open = 7,

  // Leave at end
  /** Number of session types. */
  ActivitySessionTypeCount,
  /** Invalid session type. */
  ActivitySessionType_Invalid = ActivitySessionTypeCount,
} ActivitySessionType;

/**
 * @brief Sleep state.
 *
 * Value of @ref ActivityMetricSleepState.
 */
typedef enum {
  /** Awake. */
  ActivitySleepStateAwake = 0,
  /** Restful (deep) sleep. */
  ActivitySleepStateRestfulSleep,
  /** Light sleep. */
  ActivitySleepStateLightSleep,
  /** Unknown. */
  ActivitySleepStateUnknown,
} ActivitySleepState;

/**
 * @brief Data of step based sessions (walk, run, open workout).
 *
 * Part of the data logging format: changing it requires bumping
 * @c ACTIVITY_SESSION_LOGGING_VERSION.
 */
typedef struct PBL_PACKED {
  /** Steps taken. */
  uint16_t steps;
  /** Active kcal burned. */
  uint16_t active_kcalories;
  /** Resting kcal burned. */
  uint16_t resting_kcalories;
  /** Distance covered, in meters. */
  uint16_t distance_meters;
} ActivitySessionDataStepping;

/**
 * @brief Data of sleep sessions (currently none).
 *
 * Part of the data logging format: changing it requires bumping
 * @c ACTIVITY_SESSION_LOGGING_VERSION.
 */
typedef struct {
} ActivitySessionDataSleeping;

/** @brief Maximum length of a session, in minutes (one day). */
#define ACTIVITY_SESSION_MAX_LENGTH_MIN PBL_MIN_PER_DAY

/** @brief A detected or manual activity session. */
typedef struct PBL_PACKED {
  /** Start time, UTC. */
  time_t start_utc;
  /** Length, in minutes. */
  uint16_t length_min;
  /** Session type. */
  ActivitySessionType type : 8;
  /** Session flags. */
  union {
    /** Individual flags. */
    struct {
      /** Session is still ongoing. */
      uint8_t ongoing : 1;
      /** Session was started manually (workout). */
      uint8_t manual : 1;
      /** Reserved. */
      uint8_t reserved : 6;
    };
    /** All flags. */
    uint8_t flags;
  };
  /** Type specific data. */
  union {
    /** Data of step based sessions. */
    ActivitySessionDataStepping step_data;
    /** Data of sleep sessions. */
    ActivitySessionDataSleeping sleep_data;
  };
} ActivitySession;

/**
 * @brief Version of @ref ActivityRawSamplesRecord.
 *
 * Each 32-bit entry of a record encodes a run of identical samples: bits 31-30 hold the run
 * size, then 10 bits per axis for x, y and z (most to least significant). An axis value is the
 * 16-bit raw value (mG) rounded and shifted right by 3 bits, since the dynamic range is
 * +/-4000 mG and the 3 least significant bits are mostly noise.
 */
#define ACTIVITY_RAW_SAMPLES_VERSION 2
/** @brief Maximum number of encoded entries in a @ref ActivityRawSamplesRecord. */
#define ACTIVITY_RAW_SAMPLES_MAX_ENTRIES 25

/** @brief Bits per encoded axis value. */
#define ACTIVITY_RAW_SAMPLE_VALUE_BITS (10)
/** @brief Mask of an encoded axis value. */
#define ACTIVITY_RAW_SAMPLE_VALUE_MASK (0x03FF)

/** @brief Right shift applied to raw axis values before encoding. */
#define ACTIVITY_RAW_SAMPLE_SHIFT 3
/**
 * @brief Encode a raw axis value, rounding to nearest.
 *
 * @param x Raw axis value, in mG.
 */
#define ACTIVITY_RAW_SAMPLE_VALUE_ENCODE(x) \
  ((((x) + 4) >> ACTIVITY_RAW_SAMPLE_SHIFT) & ACTIVITY_RAW_SAMPLE_VALUE_MASK)

/** @brief Maximum run size of an encoded entry. */
#define ACTIVITY_RAW_SAMPLE_MAX_RUN_SIZE 3
/**
 * @brief Get the run size of an encoded entry.
 *
 * @param s Encoded entry.
 */
#define ACTIVITY_RAW_SAMPLE_GET_RUN_SIZE(s) ((s) >> (3 * ACTIVITY_RAW_SAMPLE_VALUE_BITS))
/**
 * @brief Set the run size of an encoded entry.
 *
 * @param s Encoded entry (lvalue).
 * @param r Run size.
 */
#define ACTIVITY_RAW_SAMPLE_SET_RUN_SIZE(s, r) (s |= (r) << (3 * ACTIVITY_RAW_SAMPLE_VALUE_BITS))
/**
 * @brief Sign extend a decoded 13-bit axis value.
 *
 * @param x Decoded value, already shifted back left.
 */
#define ACTIVITY_RAW_SAMPLE_SIGN_EXTEND(x) ((x) & 0x1000 ? -1 * (0x2000 - (x)) : (x))

/**
 * @brief Decode the x axis of an encoded entry, in mG.
 *
 * @param s Encoded entry.
 */
#define ACTIVITY_RAW_SAMPLE_GET_X(s)                                                           \
  ACTIVITY_RAW_SAMPLE_SIGN_EXTEND(                                                             \
      (((uint32_t)s >> (2 * ACTIVITY_RAW_SAMPLE_VALUE_BITS)) & ACTIVITY_RAW_SAMPLE_VALUE_MASK) \
      << ACTIVITY_RAW_SAMPLE_SHIFT)
/**
 * @brief Decode the y axis of an encoded entry, in mG.
 *
 * @param s Encoded entry.
 */
#define ACTIVITY_RAW_SAMPLE_GET_Y(s)                                           \
  ACTIVITY_RAW_SAMPLE_SIGN_EXTEND(                                             \
      ((s >> ACTIVITY_RAW_SAMPLE_VALUE_BITS) & ACTIVITY_RAW_SAMPLE_VALUE_MASK) \
      << ACTIVITY_RAW_SAMPLE_SHIFT)
/**
 * @brief Decode the z axis of an encoded entry, in mG.
 *
 * @param s Encoded entry.
 */
#define ACTIVITY_RAW_SAMPLE_GET_Z(s) \
  ACTIVITY_RAW_SAMPLE_SIGN_EXTEND((s & ACTIVITY_RAW_SAMPLE_VALUE_MASK) << ACTIVITY_RAW_SAMPLE_SHIFT)

/**
 * @brief Encode a run of identical samples into an entry.
 *
 * @param run_size Number of samples in the run, up to @ref ACTIVITY_RAW_SAMPLE_MAX_RUN_SIZE.
 * @param x Raw x axis value, in mG.
 * @param y Raw y axis value, in mG.
 * @param z Raw z axis value, in mG.
 */
#define ACTIVITY_RAW_SAMPLE_ENCODE(run_size, x, y, z)                                 \
  ((run_size) << (3 * ACTIVITY_RAW_SAMPLE_VALUE_BITS)) |                              \
      (ACTIVITY_RAW_SAMPLE_VALUE_ENCODE(x) << (2 * ACTIVITY_RAW_SAMPLE_VALUE_BITS)) | \
      (ACTIVITY_RAW_SAMPLE_VALUE_ENCODE(y) << ACTIVITY_RAW_SAMPLE_VALUE_BITS) |       \
      ACTIVITY_RAW_SAMPLE_VALUE_ENCODE(z)

/** @brief @ref ActivityRawSamplesRecord flag: first record of a session. */
#define ACTIVITY_RAW_SAMPLE_FLAG_FIRST_RECORD 0x01
/** @brief @ref ActivityRawSamplesRecord flag: last record of a session. */
#define ACTIVITY_RAW_SAMPLE_FLAG_LAST_RECORD 0x02
/**
 * @brief Data logging record of raw accelerometer sample collection.
 *
 * See @ref ACTIVITY_RAW_SAMPLES_VERSION for the entry encoding.
 */
typedef struct PBL_PACKED {
  /** @ref ACTIVITY_RAW_SAMPLES_VERSION. */
  uint16_t version;
  /** Raw sample collection session id. */
  uint16_t session_id;
  /** Local time. */
  uint32_t time_local;
  /** ACTIVITY_RAW_SAMPLE_FLAG_* flags. */
  uint8_t flags;
  /** Length of this record in bytes, header included. */
  uint8_t len;
  /** Number of samples the entries expand to. */
  uint8_t num_samples;
  /** Number of valid elements in @ref entries. */
  uint8_t num_entries;
  /** Encoded entries, each representing a run of up to 3 identical samples. */
  uint32_t entries[ACTIVITY_RAW_SAMPLES_MAX_ENTRIES];
} ActivityRawSamplesRecord;

/**
 * @brief Initialize the activity service.
 *
 * Does not start tracking, see activity_start_tracking().
 *
 * @return true on success.
 */
bool activity_init(void);

/**
 * @brief Check whether the activity service is initialized.
 *
 * @return true if activity_init() succeeded.
 */
bool activity_is_initialized(void);

/**
 * @brief Start activity tracking, sampling the accelerometer.
 *
 * Tracking starts asynchronously on KernelBG.
 *
 * @param test_mode If true, the accelerometer is not used and samples must be fed with
 * activity_test_feed_samples().
 * @return true if the start request was queued.
 */
bool activity_start_tracking(bool test_mode);

/**
 * @brief Stop activity tracking.
 *
 * Tracking stops asynchronously on KernelBG.
 *
 * @return true if the stop request was queued.
 */
bool activity_stop_tracking(void);

/**
 * @brief Check whether activity tracking is running.
 *
 * @return true if tracking is currently running.
 */
bool activity_tracking_on(void);

/**
 * @brief Enable or disable the activity service for the current run level.
 *
 * Only for the service manager's services_set_runlevel(). While disabled, tracking is off
 * regardless of activity_start_tracking() and activity_stop_tracking(), and resumes when
 * re-enabled if it was started.
 *
 * @param enable Whether the run level allows the service.
 */
void activity_set_enabled(bool enable);

// Functions for getting and setting the activity preferences (defined in shell/normal/prefs.c)

/**
 * @brief Persist whether activity tracking is enabled.
 *
 * @param enable If true, enable activity tracking.
 */
void activity_prefs_tracking_set_enabled(bool enable);

/**
 * @brief Check whether activity tracking is enabled in prefs.
 *
 * @return true if enabled.
 */
bool activity_prefs_tracking_is_enabled(void);

/**
 * @brief Record the activation time, if not recorded yet.
 *
 * Used to send insights a number of days after activation.
 */
void activity_prefs_set_activated(void);

/**
 * @brief Get the activation time.
 *
 * @return UTC time of the first activity_prefs_set_activated() call, 0 if never called.
 */
time_t activity_prefs_get_activation_time(void);

/** @brief Forward declaration of @ref ActivationDelayInsightType. */
typedef enum ActivationDelayInsightType ActivationDelayInsightType;

/**
 * @brief Check whether an activation delay insight has fired.
 *
 * @param type Insight.
 * @return true if it has fired.
 */
bool activity_prefs_has_activation_delay_insight_fired(ActivationDelayInsightType type);

/**
 * @brief Mark an activation delay insight as fired.
 *
 * @param type Insight.
 */
void activity_prefs_set_activation_delay_insight_fired(ActivationDelayInsightType type);

/**
 * @brief Get the version of the health app that was last opened.
 *
 * @return Version, 0 if never opened.
 */
uint8_t activity_prefs_get_health_app_opened_version(void);

/**
 * @brief Record that the health app was opened.
 *
 * @param version Health app version.
 */
void activity_prefs_set_health_app_opened_version(uint8_t version);

/**
 * @brief Get the version of the workout app that was last opened.
 *
 * @return Version, 0 if never opened.
 */
uint8_t activity_prefs_get_workout_app_opened_version(void);

/**
 * @brief Record that the workout app was opened.
 *
 * @param version Workout app version.
 */
void activity_prefs_set_workout_app_opened_version(uint8_t version);

/**
 * @brief Enable or disable activity insights.
 *
 * @param enable If true, enable activity insights.
 */
void activity_prefs_activity_insights_set_enabled(bool enable);

/**
 * @brief Check whether activity insights are enabled.
 *
 * @return true if enabled.
 */
bool activity_prefs_activity_insights_are_enabled(void);

/**
 * @brief Enable or disable sleep insights.
 *
 * @param enable If true, enable sleep insights.
 */
void activity_prefs_sleep_insights_set_enabled(bool enable);

/**
 * @brief Check whether sleep insights are enabled.
 *
 * @return true if enabled.
 */
bool activity_prefs_sleep_insights_are_enabled(void);

/**
 * @brief Set the user's height.
 *
 * @param height_mm Height, in millimeters.
 */
void activity_prefs_set_height_mm(uint16_t height_mm);

/**
 * @brief Get the user's height.
 *
 * @return Height, in millimeters.
 */
uint16_t activity_prefs_get_height_mm(void);

/**
 * @brief Set the user's weight.
 *
 * @param weight_dag Weight, in decagrams (10 g).
 */
void activity_prefs_set_weight_dag(uint16_t weight_dag);

/**
 * @brief Get the user's weight.
 *
 * @return Weight, in decagrams (10 g).
 */
uint16_t activity_prefs_get_weight_dag(void);

/**
 * @brief Set the user's gender.
 *
 * @param gender Gender.
 */
void activity_prefs_set_gender(ActivityGender gender);

/**
 * @brief Get the user's gender.
 *
 * @return Gender.
 */
ActivityGender activity_prefs_get_gender(void);

/**
 * @brief Set the user's age.
 *
 * @param age_years Age, in years.
 */
void activity_prefs_set_age_years(uint8_t age_years);

/**
 * @brief Get the user's age.
 *
 * @return Age, in years.
 */
uint8_t activity_prefs_get_age_years(void);

/**
 * @brief Get the user's resting heart rate.
 *
 * @return Heart rate, in beats per minute.
 */
uint8_t activity_prefs_heart_get_resting_hr(void);

/**
 * @brief Get the heart rate at or above which the rate is considered elevated.
 *
 * @return Heart rate, in beats per minute.
 */
uint8_t activity_prefs_heart_get_elevated_hr(void);

/**
 * @brief Get the user's maximum heart rate.
 *
 * @return Heart rate, in beats per minute.
 */
uint8_t activity_prefs_heart_get_max_hr(void);

/**
 * @brief Get the lowest heart rate of zone 1.
 *
 * @return Heart rate, in beats per minute.
 */
uint8_t activity_prefs_heart_get_zone1_threshold(void);

/**
 * @brief Get the lowest heart rate of zone 2.
 *
 * @return Heart rate, in beats per minute.
 */
uint8_t activity_prefs_heart_get_zone2_threshold(void);

/**
 * @brief Get the lowest heart rate of zone 3.
 *
 * @return Heart rate, in beats per minute.
 */
uint8_t activity_prefs_heart_get_zone3_threshold(void);

/**
 * @brief Check whether heart rate monitoring is enabled.
 *
 * @return true if enabled.
 */
bool activity_prefs_heart_rate_is_enabled(void);

/**
 * @brief Check whether background blood oxygen (SpO2) monitoring is enabled.
 *
 * Declared regardless of @c CONFIG_HRM since the HRM manager gates sensor power on it.
 *
 * @return true if enabled.
 */
bool activity_prefs_blood_oxygen_is_enabled(void);

/**
 * @brief Check whether SpO2 sampling during detected activities is enabled.
 *
 * Opt-in, only meaningful with heart rate tracking during activities. Allows the SpO2 path even
 * when background SpO2 monitoring is off. Declared regardless of @c CONFIG_HRM.
 *
 * @return true if enabled.
 */
bool activity_prefs_blood_oxygen_activity_tracking_is_enabled(void);

#ifdef CONFIG_HRM
/**
 * @brief Get the background heart rate measurement interval.
 *
 * @return Interval; @ref HRMonitoringInterval_10Min if the stored value is invalid.
 */
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void);

/**
 * @brief Set the background heart rate measurement interval.
 *
 * @param interval Interval.
 */
void activity_prefs_set_hrm_measurement_interval(HRMonitoringInterval interval);

/**
 * @brief Check whether heart rate tracking during detected walks and runs is enabled.
 *
 * @return true if enabled.
 */
bool activity_prefs_hrm_activity_tracking_is_enabled(void);

/**
 * @brief Enable or disable heart rate tracking during detected walks and runs.
 *
 * @param enabled If true, enable it.
 */
void activity_prefs_set_hrm_activity_tracking_enabled(bool enabled);

/**
 * @brief Enable or disable background blood oxygen (SpO2) monitoring.
 *
 * The setting is synced to the phone.
 *
 * @param enabled If true, enable it.
 */
void activity_prefs_set_blood_oxygen_enabled(bool enabled);

/**
 * @brief Enable or disable SpO2 sampling during detected activities.
 *
 * @param enabled If true, enable it.
 */
void activity_prefs_set_blood_oxygen_activity_tracking_enabled(bool enabled);

/**
 * @brief Get the background SpO2 measurement interval.
 *
 * @return Interval; @ref HRMonitoringInterval_10Min if the stored value is invalid.
 */
HRMonitoringInterval activity_prefs_get_spo2_measurement_interval(void);

/**
 * @brief Set the background SpO2 measurement interval.
 *
 * @param interval Interval.
 */
void activity_prefs_set_spo2_measurement_interval(HRMonitoringInterval interval);
#endif

/**
 * @brief Get the current and, optionally, past values of a metric.
 *
 * Metrics without history only fill index 0.
 *
 * @param metric Metric to fetch.
 * @param history_len Number of entries in @p history. At most @ref ACTIVITY_HISTORY_DAYS are
 * used.
 * @param[out] history Values: today at index 0, yesterday at index 1, etc. Days without data,
 * and entries past index 0 for metrics without history, are set to -1.
 * @return true on success, false on failure.
 */
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history);

/**
 * @brief Get the typical value of a metric for a day of the week.
 *
 * Typical values are computed by the phone and stored in the health database. Only sleep
 * metrics (total, restful, enter and exit times) are available.
 *
 * @param metric Metric.
 * @param day Day of the week.
 * @param[out] value_out Typical value, 0 if unavailable.
 * @return true if a value was found.
 */
bool activity_get_metric_typical(ActivityMetric metric, enum pbl_weekday day, int32_t *value_out);

/**
 * @brief Get the average value of a metric over the last 4 weeks.
 *
 * Only @ref ActivityMetricStepCount and @ref ActivityMetricSleepTotalSeconds are available,
 * as stored in the health database by the phone.
 *
 * @param metric Metric.
 * @param[out] value_out Average value, 0 if unavailable.
 * @return true if a value was found.
 */
bool activity_get_metric_monthly_avg(ActivityMetric metric, int32_t *value_out);

/**
 * @brief Get the cached activity sessions.
 *
 * Sessions older than the current day are pruned once a minute, so these are today's sessions.
 *
 * @param[in,out] session_entries On entry, capacity of @p sessions in elements. On exit,
 * number of sessions written.
 * @param[out] sessions Filled with the sessions.
 * @return true on success, false on failure.
 */
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions);

/**
 * @brief Get historical minute data.
 *
 * Blocks on KernelBG, so only callable from the app or worker task.
 *
 * @param[out] minute_data Array filled with one record per minute.
 * @param[in,out] num_records On entry, capacity of @p minute_data. On exit, number of records
 * written, missing minutes included (marked with @c is_invalid).
 * @param[in,out] utc_start On entry, UTC time of the first requested minute. On exit, UTC time
 * of the first returned minute.
 * @return true on success, false on failure.
 */
bool activity_get_minute_history(HealthMinuteData *minute_data, uint32_t *num_records,
                                 time_t *utc_start);

/** @brief Number of step averages per day, one per 15 minute interval. */
#define ACTIVITY_NUM_METRIC_AVERAGES (4 * 24)
/** @brief Step average value meaning unknown. */
#define ACTIVITY_METRIC_AVERAGES_UNKNOWN 0xFFFF
/** @brief Typical steps for each 15 minute interval of a day. */
typedef struct {
  /** Typical steps per interval from midnight, @ref ACTIVITY_METRIC_AVERAGES_UNKNOWN if unknown. */
  uint16_t average[ACTIVITY_NUM_METRIC_AVERAGES];
} ActivityMetricAverages;

/**
 * @brief Get the typical step counts of a day of the week.
 *
 * Read from the health database, populated by the phone.
 *
 * @param day_of_week Day of the week.
 * @param[out] averages Step averages; all unknown if unavailable.
 * @return true on success, false on failure.
 */
bool activity_get_step_averages(enum pbl_weekday day_of_week, ActivityMetricAverages *averages);

/**
 * @brief Control raw accelerometer sample collection.
 *
 * Collected samples are sent to data logging as @ref ActivityRawSamplesRecord records and also
 * logged base64-encoded with PBL_LOG, so they can be retrieved through a support request. Each
 * enable starts a new session id, which can be shown to the user to identify the session.
 *
 * @param enable If true, enable sample collection.
 * @param disable If true, disable sample collection.
 * @param[out] enabled Whether sample collection is enabled.
 * @param[out] session_id Current session id, or the last one if collection is disabled.
 * @param[out] num_samples Samples collected in the current, or last, session.
 * @param[out] seconds Seconds of data collected in the current, or last, session.
 * @return true on success, false on error.
 */
bool activity_raw_sample_collection(bool enable, bool disable, bool *enabled, uint32_t *session_id,
                                    uint32_t *num_samples, uint32_t *seconds);

/**
 * @brief Dump the minute data used for sleep base64-encoded with PBL_LOG.
 *
 * Used to extract it through a support request. Blocks on KernelBG, so only callable from the
 * app or worker task.
 *
 * @return true on success, false on error.
 */
bool activity_dump_sleep_log(void);

/**
 * @brief Feed accelerometer samples, bypassing the accelerometer (test apps).
 *
 * Requires tracking started with activity_start_tracking() in test mode.
 *
 * @param data Samples.
 * @param num_samples Number of samples in @p data.
 * @return true on success, false on error.
 */
bool activity_test_feed_samples(AccelRawData *data, uint32_t num_samples);

/**
 * @brief Run the minute callback immediately (test apps).
 *
 * Allows running tests faster than real time.
 *
 * @return true on success, false on error.
 */
bool activity_test_run_minute_callback(void);

/**
 * @brief Get information on the minute data file (test apps).
 *
 * @param compact_first If true, compact the file first.
 * @param[out] num_records Number of records in the file.
 * @param[out] data_bytes Bytes of data in the file.
 * @param[out] minutes Minutes of data in the file.
 * @return true on success, false on error.
 */
bool activity_test_minute_file_info(bool compact_first, uint32_t *num_records, uint32_t *data_bytes,
                                    uint32_t *minutes);

/**
 * @brief Fill the minute data file (test apps).
 *
 * Used to test compaction performance and watchdog timeouts with a large file.
 *
 * @return true on success, false on error.
 */
bool activity_test_fill_minute_file(void);

/**
 * @brief Send fake records to data logging (test apps).
 *
 * Sends an AlgMinuteDLSRecord, an ActivityLegacySleepSessionDataLoggingRecord and an
 * ActivitySessionDataLoggingRecord for each session type. Useful for mobile app testing.
 *
 * @return true on success.
 */
bool activity_test_send_fake_dls_records(void);

/**
 * @brief Set the current step count and averages (test apps).
 *
 * @param new_steps Steps taken today.
 * @param current_avg Typical steps up to the current time.
 * @param daily_avg Typical steps for the whole day.
 */
void activity_test_set_steps_and_avg(int32_t new_steps, int32_t current_avg, int32_t daily_avg);

/** @brief Fill the past days of step history with test data (test apps). */
void activity_test_set_steps_history();

/** @brief Fill the past days of sleep history with test data (test apps). */
void activity_test_set_sleep_history();

/** @} */
