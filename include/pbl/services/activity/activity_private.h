/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "activity.h"
#include "hr_util.h"

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/logging/logging.h>
#include <pbl/services/data_logging/data_logging_service.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/util/time.h>
#include <pbl/util/units.h>

#include <applib/event_service_client.h>
#include <kernel/events.h>
#include <system/hexdump.h>

/**
 * @defgroup services_activity_activity_private Activity service internals
 * @ingroup services_activity
 * @brief State, storage formats and helpers shared by the activity service sources.
 *
 * Only for the activity service itself (activity.c, activity_metrics.c, activity_sessions.c,
 * activity_insights.c), the workout service and their tests. Unless noted, functions must be
 * called with the activity state mutex held.
 * @{
 */

/** @brief Interval between saves of today's metrics to the settings file, in minutes. */
#define ACTIVITY_SETTINGS_UPDATE_MIN 15

/** @brief Interval between recomputations of the activity sessions, in minutes. */
#define ACTIVITY_SESSION_UPDATE_MIN 15

/**
 * @brief Storage type of every scalar metric and setting, in RAM and in the settings file.
 *
 * Wide enough for daily step counts and distances in meters above UINT16_MAX.
 */
typedef uint32_t ActivityScalarStore;
/** @brief Maximum value of @ref ActivityScalarStore. */
#define ACTIVITY_SCALAR_MAX UINT32_MAX

/** @brief Minutes covered by each step average. */
#define ACTIVITY_STEP_AVERAGES_MINUTES (PBL_MIN_PER_DAY / ACTIVITY_NUM_METRIC_AVERAGES)

/**
 * @brief Step averages stored per settings key.
 *
 * Trades flash writes against the data lost on reset.
 */
#define ACTIVITY_STEP_AVERAGES_PER_KEY 4
/** @brief Settings keys holding one day of step averages. */
#define ACTIVITY_STEP_AVERAGES_KEYS_PER_DAY \
  (ACTIVITY_NUM_METRIC_AVERAGES / ACTIVITY_STEP_AVERAGES_PER_KEY)

/** @brief Minimum steps in a minute for it to be an active minute. */
#define ACTIVITY_ACTIVE_MINUTE_MIN_STEPS 40

/**
 * @brief Minute of the day (9pm) after which an ending sleep session counts as the next day's.
 */
#define ACTIVITY_LAST_SLEEP_MINUTE_OF_DAY (21 * PBL_MIN_PER_HOUR)

/** @brief Default length of a background heart rate measurement window, in seconds. */
#define ACTIVITY_DEFAULT_HR_ON_TIME_SEC (60)

/**
 * @brief Time into a heart rate window after which it is aborted without any valid reading, in
 * seconds.
 *
 * The HR model converges in about 9 s, so no reading by then means poor contact, off-wrist or
 * heavy motion. Any reading, even of acceptable quality, keeps the window running.
 */
#define ACTIVITY_HR_EARLY_ABORT_SEC (40)

/** @brief Good quality heart rate samples after which the window ends early. */
#define ACTIVITY_MIN_NUM_GOOD_SAMPLES_SHORT_CIRCUIT (10)

/** @brief Excellent quality heart rate samples after which the window ends early. */
#define ACTIVITY_MIN_NUM_EXCELLENT_SAMPLES_SHORT_CIRCUIT (5)

/**
 * @brief Good quality SpO2 samples after which the window ends early.
 *
 * Kept low: good readings are hard to get on the wrist.
 */
#define ACTIVITY_MIN_NUM_GOOD_SPO2_SAMPLES_SHORT_CIRCUIT (2)

/**
 * @brief Maximum length of a background SpO2 measurement window, in seconds.
 *
 * Longer than the heart rate window since SpO2 typically needs about 30 s to converge; bounds
 * the high-current red/IR LED on-time when no reading comes.
 */
#define ACTIVITY_DEFAULT_SPO2_ON_TIME_SEC (90)

/**
 * @brief Time into a SpO2 window after which it is aborted if no sample was accepted, in
 * seconds.
 *
 * A converging reading gets accepted samples well before; none means poor contact, motion or
 * off-wrist.
 */
#define ACTIVITY_SPO2_EARLY_ABORT_SEC (45)

/** @brief Interval between SpO2 attempts during a detected activity, in seconds. */
#define ACTIVITY_SPO2_ACTIVITY_INTERVAL_SEC (5 * PBL_SEC_PER_MIN)
/** @brief Duration of a SpO2 attempt during a detected activity, in seconds. */
#define ACTIVITY_SPO2_ACTIVITY_ATTEMPT_SEC (35)
/** @brief Heart rate time after a failed SpO2 attempt before retrying, in seconds. */
#define ACTIVITY_SPO2_ACTIVITY_BACKOFF_SEC (PBL_SEC_PER_MIN)

/** @brief Minimum heart rate samples in a minute to report a zone above zone 0. */
#define ACTIVITY_MIN_NUM_SAMPLES_FOR_HR_ZONE (5)

/** @brief Heart rate subscription interval while sampling, in seconds. */
#define ACTIVITY_HRM_SUBSCRIPTION_ON_PERIOD_SEC (1)
/** @brief Heart rate subscription interval while idle, in seconds. */
#define ACTIVITY_HRM_SUBSCRIPTION_OFF_PERIOD_SEC (PBL_SEC_PER_DAY)

/**
 * @brief Age after which the cached off-wrist status is stale, in seconds.
 *
 * Sleep detection then falls back to its accelerometer based not-worn heuristics. Covers the
 * default 10 minute heart rate interval; with longer intervals the cache expires between
 * measurements.
 */
#define ACTIVITY_HRM_OFFWRIST_STALE_SEC (15 * PBL_SEC_PER_MIN)

/** @brief Maximum number of heart rate samples kept to compute the median. */
#define ACTIVITY_MAX_HR_SAMPLES (3 * PBL_SEC_PER_MIN)

/** @brief Decagrams per kilogram. */
#define ACTIVITY_DAG_PER_KG 100

/** @brief Name of the activity settings file. */
#define ACTIVITY_SETTINGS_FILE_NAME "activity"
/** @brief Size of the activity settings file, in bytes. */
#define ACTIVITY_SETTINGS_FILE_LEN 0x4000

/**
 * @brief Version of the activity settings file.
 *
 * - 1: no @ref ActivitySettingsKeyVersion;
 * - 2: file size changed from 2 KiB to 16 KiB;
 * - 3: @ref ActivityScalarStore widened from 16 to 32 bits, changing history and scalar records.
 */
#define ACTIVITY_SETTINGS_CURRENT_VERSION 3

/** @brief History of a metric, as stored in the settings file. */
typedef struct {
  /** UTC time of the first entry. */
  uint32_t utc_sec;
  /** One value per day, today at index 0. */
  ActivityScalarStore values[ACTIVITY_HISTORY_DAYS];
} ActivitySettingsValueHistory;

/**
 * @brief Keys of the activity settings file.
 *
 * Values are persisted: new keys go at the end.
 */
typedef enum {
  /** Invalid key. */
  ActivitySettingsKeyInvalid = 0,
  /** uint16_t: @ref ACTIVITY_SETTINGS_CURRENT_VERSION. */
  ActivitySettingsKeyVersion,
  /** Unused. */
  ActivitySettingsKeyUnused0,
  /** Unused. */
  ActivitySettingsKeyUnused1,
  /** Unused. */
  ActivitySettingsKeyUnused2,
  /** Unused. */
  ActivitySettingsKeyUnused3,

  /** Steps (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeyStepCountHistory,
  /** Active minutes (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeyStepMinutesHistory,
  /** Unused. */
  ActivitySettingsKeyUnused4,
  /** Distance, in meters (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeyDistanceMetersHistory,
  /** Sleep, in minutes (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeySleepTotalMinutesHistory,
  /** Restful sleep, in minutes (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeySleepDeepMinutesHistory,
  /** Minutes it took to fall asleep (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeySleepEntryMinutesHistory,
  /** Time the user fell asleep, in minutes after midnight (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeySleepEnterAtHistory,
  /** Time the user woke up, in minutes after midnight (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeySleepExitAtHistory,
  /** @ref ActivitySleepState (@ref ActivityScalarStore). */
  ActivitySettingsKeySleepState,
  /** Minutes in the current sleep state (@ref ActivityScalarStore). */
  ActivitySettingsKeySleepStateMinutes,
  /** First key of the weekday step averages, @ref ACTIVITY_STEP_AVERAGES_PER_KEY uint16_t each. */
  ActivitySettingsKeyStepAveragesWeekdayFirst,
  /** Last key of the weekday step averages. */
  ActivitySettingsKeyStepAveragesWeekdayLast =
      ActivitySettingsKeyStepAveragesWeekdayFirst + ACTIVITY_STEP_AVERAGES_KEYS_PER_DAY - 1,

  /** First key of the weekend step averages, @ref ACTIVITY_STEP_AVERAGES_PER_KEY uint16_t each. */
  ActivitySettingsKeyStepAveragesWeekendFirst,
  /** Last key of the weekend step averages. */
  ActivitySettingsKeyStepAveragesWeekendLast =
      ActivitySettingsKeyStepAveragesWeekendFirst + ACTIVITY_STEP_AVERAGES_KEYS_PER_DAY - 1,
  /** uint16_t: age, in years. */
  ActivitySettingsKeyAgeYears,

  /** Unused. */
  ActivitySettingsKeyUnused5,

  /** time_t: last time the sleep reward was shown, 0 if never. */
  ActivitySettingsKeyInsightSleepRewardTime,
  /** time_t: last time the activity reward was shown, 0 if never. */
  ActivitySettingsKeyInsightActivityRewardTime,
  /** SummaryPinLastState: UUID and last time the activity summary pin was added. */
  ActivitySettingsKeyInsightActivitySummaryState,
  /** SummaryPinLastState: UUID and last time the sleep summary pin was added. */
  ActivitySettingsKeyInsightSleepSummaryState,
  /** Resting kcal (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeyRestingKCaloriesHistory,
  /** Active kcal (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeyActiveKCaloriesHistory,
  /** time_t: UTC time of the last sleep activity logged to analytics. */
  ActivitySettingsKeyLastSleepActivityUTC,
  /** time_t: UTC time of the last restful sleep activity logged to analytics. */
  ActivitySettingsKeyLastRestfulSleepActivityUTC,
  /** time_t: UTC time of the last step activity logged to analytics. */
  ActivitySettingsKeyLastStepActivityUTC,
  /** @ref ActivitySession[@ref ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT]: stored sessions. */
  ActivitySettingsKeyStoredActivities,
  /** time_t: last time the nap pin was shown. */
  ActivitySettingsKeyInsightNapSessionTime,
  /** time_t: last time the activity session pin was shown. */
  ActivitySettingsKeyInsightActivitySessionTime,
  /** VMC of the last processed minute (@ref ActivityScalarStore). */
  ActivitySettingsKeyLastVMC,
  /** Resting heart rate (@ref ActivitySettingsValueHistory). */
  ActivitySettingsKeyRestingHeartRate,
  /** Minutes in heart rate zone 1 today (@ref ActivityScalarStore). */
  ActivitySettingsKeyHeartRateZone1Minutes,
  /** Minutes in heart rate zone 2 today (@ref ActivityScalarStore). */
  ActivitySettingsKeyHeartRateZone2Minutes,
  /** Minutes in heart rate zone 3 today (@ref ActivityScalarStore). */
  ActivitySettingsKeyHeartRateZone3Minutes,
} ActivitySettingsKey;

/**
 * @brief Today's step metrics.
 *
 * Every member must be an @ref ActivityScalarStore (see activity_metrics_prv_get_metric_info()).
 */
typedef struct {
  /** Steps. */
  ActivityScalarStore steps;
  /** Active minutes. */
  ActivityScalarStore step_minutes;
  /** Distance, in meters. */
  ActivityScalarStore distance_meters;
  /** Resting kcal. */
  ActivityScalarStore resting_kcalories;
  /** Active kcal. */
  ActivityScalarStore active_kcalories;
} ActivityStepData;

/**
 * @brief Today's sleep metrics.
 *
 * Every member must be an @ref ActivityScalarStore (see activity_metrics_prv_get_metric_info()).
 */
typedef struct {
  /** Sleep, in minutes. */
  ActivityScalarStore total_minutes;
  /** Restful sleep, in minutes. */
  ActivityScalarStore restful_minutes;
  /** Time the user fell asleep, in minutes after midnight. */
  ActivityScalarStore enter_at_minute;
  /** Time the user woke up, in minutes after midnight. */
  ActivityScalarStore exit_at_minute;
  /** Current @ref ActivitySleepState. */
  ActivityScalarStore cur_state;
  /** Minutes in the current sleep state. */
  ActivityScalarStore cur_state_elapsed_minutes;
} ActivitySleepData;

/**
 * @brief Heart rate metrics.
 *
 * Members are @ref ActivityScalarStore except the update times, which are 32-bit metrics
 * without history that are not persisted.
 */
typedef struct {
  /** Most recent reading, in beats per minute. */
  ActivityScalarStore current_bpm;
  /** UTC time of the most recent reading. */
  uint32_t current_update_time_utc;
  /** Current @ref HRZone. */
  ActivityScalarStore current_hr_zone;
  /** Resting heart rate, in beats per minute. */
  ActivityScalarStore resting_bpm;
  /** Quality of the most recent reading, an @c HRMQuality value. */
  ActivityScalarStore current_quality;
  /** Last stable (median) heart rate, in beats per minute. */
  ActivityScalarStore last_stable_bpm;
  /** UTC time of the last stable heart rate. */
  uint32_t last_stable_bpm_update_time_utc;
  /** Median heart rate of the last minute, in beats per minute. */
  ActivityScalarStore previous_median_bpm;
  /** Total weight of the last minute's samples, multiplied by 100. */
  int32_t previous_median_total_weight_x100;
  /** Minutes in each @ref HRZone today. */
  ActivityScalarStore minutes_in_zone[HRZoneCount];
  /** Heart rate is elevated. */
  bool is_hr_elevated;
} ActivityHeartRateData;

/**
 * @brief Convert a metric from its storage value to the activity_get_metric() value.
 *
 * For example, minutes to seconds.
 *
 * @param storage_value Stored value.
 * @return Converted value.
 */
typedef uint32_t (*ActivityMetricConverter)(ActivityScalarStore storage_value);

/** @brief Storage information of a metric, filled by activity_metrics_prv_get_metric_info(). */
typedef struct {
  /** Value in RAM. */
  ActivityScalarStore *value_p;
  /**
   * Alternative value pointer for 32-bit metrics, which have no history and use
   * @ref ActivitySettingsKeyInvalid.
   */
  uint32_t *value_u32p;
  /** Metric has history, which sets its size in the settings file. */
  bool has_history;
  /** Settings key. */
  ActivitySettingsKey settings_key;
  /** Converter from storage value to returned value. */
  ActivityMetricConverter converter;
} ActivityMetricInfo;

/** @brief Samples fed by activity_test_feed_samples(). */
typedef struct {
  /** Number of samples in @ref data. */
  uint16_t num_samples;
  /** Samples. */
  AccelRawData data[];
} ActivityFeedSamples;

/**
 * @brief Version of the legacy sleep session data logging records (before FW 3.11).
 *
 * Treated as a bitfield by the mobile app: while bit 0 is set, fields may be appended to
 * @ref ActivityLegacySleepSessionDataLoggingRecord; clearing it marks the record unparseable.
 */
#define ACTIVITY_SLEEP_SESSION_LOGGING_VERSION 1

/** @brief Legacy data logging record of a sleep session. */
typedef struct PBL_PACKED {
  /** @ref ACTIVITY_SLEEP_SESSION_LOGGING_VERSION. */
  uint16_t version;
  /** Seconds to add to UTC to get local time. */
  int32_t utc_to_local;
  /** Start time, UTC. */
  uint32_t start_utc;
  /** End time, UTC. */
  uint32_t end_utc;
  /** Seconds of restful sleep. */
  uint32_t restful_secs;
} ActivityLegacySleepSessionDataLoggingRecord;

/**
 * @brief Version of the activity session data logging records.
 *
 * Treated as a bitfield by the mobile app: while bit 0 is set, fields may be appended to
 * @ref ActivitySessionDataLoggingRecord; clearing it marks the record unparseable.
 */
#define ACTIVITY_SESSION_LOGGING_VERSION 3

/**
 * @brief Data logging record of an activity session.
 *
 * Changing it requires bumping @ref ACTIVITY_SESSION_LOGGING_VERSION.
 */
typedef struct PBL_PACKED {
  /** @ref ACTIVITY_SESSION_LOGGING_VERSION. */
  uint16_t version;
  /** Size of this record, in bytes. */
  uint16_t size;
  /** @ref ActivitySessionType. */
  uint16_t activity;
  /** Seconds to add to UTC to get local time. */
  int32_t utc_to_local;
  /** Start time, UTC. */
  uint32_t start_utc;
  /** Duration, in seconds. */
  uint32_t elapsed_sec;

  // New fields add in version 3
  /** Type specific data (version 3). */
  union {
    /** Data of step based sessions. */
    ActivitySessionDataStepping step_data;
    /** Data of sleep sessions. */
    ActivitySessionDataSleeping sleep_data;
  };
} ActivitySessionDataLoggingRecord;

/** @brief Raw accelerometer sample collection state. */
typedef struct {
  /** Data logging session of the collection session. */
  DataLoggingSession *dls_session;

  /** Last encoded sample, used to detect runs (see @ref ACTIVITY_RAW_SAMPLES_VERSION). */
  uint32_t prev_sample;
  /** Run size of @ref prev_sample. */
  uint8_t run_size;

  /** Record being built. */
  ActivityRawSamplesRecord record;

  /** Buffer to base64 encode half a record at a time. */
  char base64_buf[sizeof(ActivityRawSamplesRecord)];

  /** The first record is being built. */
  bool first_record;
} ActivitySampleCollectionData;

/**
 * @brief Measurements log handle.
 *
 * Defined in measurements_log.h, which cannot be included here because of the generated SDK
 * files.
 */
typedef void *ProtobufLogRef;

/** @brief Heart rate support state. */
typedef struct {
  /** Heart rate metrics. */
  ActivityHeartRateData metrics;

  /** HRM manager session. */
  HRMSessionRef hrm_session;
  /** Measurements log the samples are sent to. */
  ProtobufLogRef log_session;

  /** Sampling is active. */
  bool currently_sampling;
  /** Uptime, in seconds, of the last sampling toggle. */
  uint32_t toggled_sampling_at_ts;

  /** Uptime, in seconds, of the last sample. */
  uint32_t last_sample_ts;

  /** Samples in the past minute. */
  uint16_t num_samples;
  /** Good quality samples in the past minute. */
  uint16_t num_good_quality_samples;
  /** Excellent quality samples in the past minute. */
  uint16_t num_excellent_samples;
  /** Stored samples, in beats per minute. */
  uint8_t samples[ACTIVITY_MAX_HR_SAMPLES];
  /** Weights of the stored samples. */
  uint8_t weights[ACTIVITY_MAX_HR_SAMPLES];

  /** UTC time of the last BPM event, 0 if none. Used by sleep tracking to detect off-wrist. */
  time_t last_quality_event_utc;
  /** Last BPM event reported off-wrist. */
  bool last_quality_was_offwrist;
} ActivityHRSupport;

/** @brief Background SpO2 support state. */
typedef struct {
  /** HRM manager session used for SpO2. */
  HRMSessionRef hrm_session;

  /** Sampling is active. */
  bool currently_sampling;
  /** Uptime, in seconds, of the last sampling toggle. */
  uint32_t toggled_sampling_at_ts;
  /** Good quality samples in the current window. */
  uint16_t num_good_quality_samples;

  // Latest valid reading waiting to be written into the next minute record (0 = none pending).
  // The minute handler consumes and clears these, so each measurement is logged exactly once.
  /** Pending saturation for the next minute record, in percent, 0 if none. */
  uint8_t pending_percent;
  /** Pending quality on the 0-7 @c HeartRateQuality scale, 0 if none. */
  uint8_t pending_quality;
} ActivitySpO2Support;

/** @brief Phases of the SpO2 reader during detected activities. */
typedef enum {
  /** Not in an eligible activity. */
  ActivitySpO2ActPhase_Idle = 0,
  /** Sampling heart rate, waiting for the next attempt. */
  ActivitySpO2ActPhase_Wait,
  /** Heart rate paused, sampling SpO2 within the attempt budget. */
  ActivitySpO2ActPhase_Sampling,
  /** Last attempt failed; heart rate resumed before retrying. */
  ActivitySpO2ActPhase_Backoff,
} ActivitySpO2ActPhase;

/** @brief SpO2 reader state during detected activities. */
typedef struct {
  /** Dedicated SpO2 session, only sampled during an attempt. */
  HRMSessionRef hrm_session;
  /** Current phase. */
  ActivitySpO2ActPhase phase;
  /** Uptime, in seconds, at which the current phase began. */
  uint32_t phase_started_ts;
  /** A valid reading arrived during this attempt. */
  bool got_reading;
  /** A KernelBG callback running the state machine is queued. */
  bool update_pending;
} ActivitySpO2ActivitySupport;

/** @brief Activity service state. */
typedef struct {
  /** Mutex serializing access to the state. */
  struct pbl_mutex mutex;

  /** Semaphore to wait for KernelBG callbacks to finish. */
  struct pbl_sem bg_wait_semaphore;

  /** Accelerometer session. */
  AccelServiceState *accel_session;

  /** Battery state subscription, to track the charger. */
  EventServiceInfo charger_subscription;

  /** Today's step metrics. */
  ActivityStepData step_data;
  /** Today's sleep metrics. */
  ActivitySleepData sleep_data;

  // Accumulated in fine units to minimize rounding errors, since they are incremented on every
  // rate update from the algorithm (every 5 seconds).
  /** Distance today, in millimeters. */
  uint32_t distance_mm;
  /** Active calories today (1/1000 kcal). */
  uint32_t active_calories;
  /** Resting calories today (1/1000 kcal). */
  uint32_t resting_calories;
  /** VMC of the last minute. */
  ActivityScalarStore last_vmc;
  /** Orientation of the last minute. */
  uint8_t last_orientation;
  /** UTC time of the last rate update. */
  time_t rate_last_update_time;

  /** Average steps per minute over the last minute. */
  ActivityScalarStore steps_per_minute;
  /** Step count when @ref steps_per_minute was last computed. */
  ActivityScalarStore steps_per_minute_last_steps;

  /** Last minute of the day with significant steps, used to compute the time to fall asleep. */
  uint16_t last_active_minute;

  /** Heart rate support. */
  ActivityHRSupport hr;

  /** Background SpO2 support. */
  ActivitySpO2Support spo2;

  /** SpO2 during detected activities. */
  ActivitySpO2ActivitySupport activity_spo2;

  /** Current day index. */
  uint16_t cur_day_index;

  /** Minutes until the next settings file update. */
  int8_t update_settings_counter;

  /** Number of valid entries in @ref activity_sessions. */
  uint16_t activity_sessions_count;
  /** Captured sessions. */
  ActivitySession activity_sessions[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
  /** Sessions need to be persisted. */
  bool need_activities_saved;

  /** A new sleep session was registered. */
  bool sleep_sessions_modified;

  // Exit time for the last sleep/step activities we logged. Used to prevent logging the same event
  // more than once.
  /** End time of the last logged sleep activity, UTC. */
  time_t logged_sleep_activity_exit_at_utc;
  /** End time of the last logged restful sleep activity, UTC. */
  time_t logged_restful_sleep_activity_exit_at_utc;
  /** End time of the last logged step activity, UTC. */
  time_t logged_step_activity_exit_at_utc;

  /** Data logging session for activity sessions. */
  DataLoggingSession *activity_dls_session;

  /** UTC time of the first active minute of a significant activity event, 0 if none. */
  time_t activity_event_start_utc;

  /** The run level allows the service (services_set_runlevel()). */
  bool enabled_run_level;
  /** The charging state allows the service. */
  bool enabled_charging_state;

  /** Tracking should run; kept while disabled so tracking restarts when re-enabled. */
  bool should_be_started;

  /** Tracking is running; only set while enabled. */
  bool started;

  /** Raw sample collection is enabled. */
  bool sample_collection_enabled;
  /** Raw sample collection session id. */
  uint16_t sample_collection_session_id;
  /**
   * While collecting, UTC time collection started; otherwise seconds of data in the last
   * session.
   */
  time_t sample_collection_seconds;
  /** Samples collected so far. */
  uint16_t sample_collection_num_samples;
  /** Raw sample collection state, allocated while collecting. */
  ActivitySampleCollectionData *sample_collection_data;

  /** activity_start_tracking() was called in test mode. */
  bool test_mode;
  /** A test feed callback is pending on KernelBG. */
  bool pending_test_cb;
} ActivityState;

/**
 * @brief Get the activity state.
 *
 * @return Activity state.
 */
ActivityState *activity_private_state(void);

/**
 * @brief Check whether a heart rate monitor is present.
 *
 * @return true if present.
 */
bool activity_is_hrm_present(void);

/**
 * @brief Open the activity settings file.
 *
 * Only call during init or while holding the activity mutex.
 *
 * @return Settings file, NULL on failure.
 */
SettingsFile *activity_private_settings_open(void);

/**
 * @brief Close the activity settings file.
 *
 * Only call during init or while holding the activity mutex.
 *
 * @param file File returned by activity_private_settings_open().
 */
void activity_private_settings_close(SettingsFile *file);

/**
 * @brief Reinitialize the activity service (test apps).
 *
 * @param reset_settings If true, clear all persistent data.
 * @param tracking_on If true, turn tracking on; otherwise keep the current state.
 * @param sleep_history If not NULL, sleep history to write.
 * @param step_history If not NULL, step history to write.
 * @return true on success.
 */
bool activity_test_reset(bool reset_settings, bool tracking_on,
                         const ActivitySettingsValueHistory *sleep_history,
                         const ActivitySettingsValueHistory *step_history);

/**
 * @brief Load the stored activity sessions.
 *
 * @param file Activity settings file.
 * @param utc_now Current UTC time.
 */
void activity_sessions_prv_init(SettingsFile *file, time_t utc_now);

/**
 * @brief Get the start of the sleep day containing a time.
 *
 * Sleep days start at @ref ACTIVITY_LAST_SLEEP_MINUTE_OF_DAY local time.
 *
 * @param now_utc UTC time.
 * @return Start of the sleep day, UTC.
 */
time_t activity_sessions_prv_get_sleep_window_start_utc(time_t now_utc);

/**
 * @brief Get the sleep bounds of the current day.
 *
 * @param now_utc Current UTC time.
 * @param[out] enter_utc Earliest sleep entry time, UTC.
 * @param[out] exit_utc Latest sleep exit time, UTC.
 */
void activity_sessions_prv_get_sleep_bounds_utc(time_t now_utc, time_t *enter_utc,
                                                time_t *exit_utc);

/**
 * @brief Remove sessions older than today or in the future.
 *
 * @param utc_sec Current UTC time.
 * @param remove_ongoing If true, also remove ongoing sessions.
 */
void activity_sessions_prv_remove_out_of_range_activity_sessions(time_t utc_sec,
                                                                 bool remove_ongoing);

/**
 * @brief Check whether a session type is sleep related.
 *
 * @param activity_type Session type.
 * @return true for sleep, restful sleep, nap and restful nap.
 */
bool activity_sessions_prv_is_sleep_activity(ActivitySessionType activity_type);

/**
 * @brief Check whether a session of a type is ongoing.
 *
 * @param activity_type Session type.
 * @return true if one is ongoing.
 */
bool activity_sessions_is_session_type_ongoing(ActivitySessionType activity_type);

/**
 * @brief Register a new session, or update an existing one.
 *
 * Called by the algorithm when it detects an activity.
 *
 * @param session Session.
 */
void activity_sessions_prv_add_activity_session(ActivitySession *session);

/**
 * @brief Delete a session.
 *
 * Called by the algorithm when it drops a session. Only ongoing sessions can be deleted.
 *
 * @param session Session.
 */
void activity_sessions_prv_delete_activity_session(ActivitySession *session);

/**
 * @brief Run the once a minute session maintenance.
 *
 * @param utc_sec Current UTC time.
 */
void activity_sessions_prv_minute_handler(time_t utc_sec);

/**
 * @brief Send a session to data logging.
 *
 * @param session Session.
 */
void activity_sessions_prv_send_activity_session_to_data_logging(ActivitySession *session);

/**
 * @brief Initialize all metrics from the settings file.
 *
 * @param file Activity settings file.
 * @param utc_now Current UTC time.
 */
void activity_metrics_prv_init(SettingsFile *file, time_t utc_now);

/**
 * @brief Get the storage information of a metric.
 *
 * @param metric Metric.
 * @param[out] info Storage information.
 */
void activity_metrics_prv_get_metric_info(ActivityMetric metric, ActivityMetricInfo *info);

/**
 * @brief Run the once a minute metrics maintenance.
 *
 * @param utc_sec Current UTC time.
 */
void activity_metrics_prv_minute_handler(time_t utc_sec);

/**
 * @brief Get the distance covered today.
 *
 * @return Distance, in millimeters.
 */
uint32_t activity_metrics_prv_get_distance_mm(void);

/**
 * @brief Get the resting calories burned today.
 *
 * @return Calories (1/1000 kcal).
 */
uint32_t activity_metrics_prv_get_resting_calories(void);

/**
 * @brief Get the active calories burned today.
 *
 * @return Calories (1/1000 kcal).
 */
uint32_t activity_metrics_prv_get_active_calories(void);

/**
 * @brief Get the median heart rate since the last reset.
 *
 * Reset every minute by activity_metrics_prv_reset_hr_stats().
 *
 * @param[out] median_out Median heart rate, in beats per minute, 0 if no readings.
 * @param[out] heart_rate_total_weight_x100_out Total weight of the readings, multiplied by 100.
 */
void activity_metrics_prv_get_median_hr_bpm(int32_t *median_out,
                                            int32_t *heart_rate_total_weight_x100_out);

/**
 * @brief Get the heart rate zone since the last reset.
 *
 * Reset every minute by activity_metrics_prv_reset_hr_stats().
 *
 * @return Zone, @ref HRZone_Zone0 if no readings.
 */
HRZone activity_metrics_prv_get_hr_zone(void);

/** @brief Reset the median heart rate and the heart rate zone. */
void activity_metrics_prv_reset_hr_stats(void);

/**
 * @brief Consume the pending SpO2 reading for the current minute record.
 *
 * Each reading is logged to exactly one minute.
 *
 * @param[out] percent_out Saturation, in percent, 0 if none pending.
 * @param[out] quality_out Quality on the 0-7 @c HeartRateQuality scale, 0 if none pending.
 */
void activity_metrics_prv_get_spo2_sample(uint8_t *percent_out, uint8_t *quality_out);

/**
 * @brief Add a heart rate sample to the median.
 *
 * @param hrm_event BPM event.
 * @param now_utc Current UTC time.
 * @param now_uptime Current uptime, in seconds.
 */
void activity_metrics_prv_add_median_hr_sample(PebbleHRMEvent *hrm_event, time_t now_utc,
                                               time_t now_uptime);

/**
 * @brief Record the worn status reported by the HRM.
 *
 * Called on every BPM event.
 *
 * @param now_utc Current UTC time.
 * @param is_offwrist true if the event quality was off-wrist.
 */
void activity_metrics_prv_set_hrm_worn_status(time_t now_utc, bool is_offwrist);

/**
 * @brief Invalidate the current (peekable) raw HR reading because the HRM
 * reports the watch off-wrist.
 *
 * ActivityMetricHeartRateRawBPM reads 0 until a valid on-wrist reading
 * arrives. Called once per off-wrist BPM event.
 *
 * @param now_utc Current UTC time.
 */
void activity_metrics_prv_set_raw_hr_offwrist(time_t now_utc);

/**
 * @brief Check whether the HRM recently reported the watch off-wrist.
 *
 * @param now_utc Current UTC time.
 * @return true if the last BPM event was off-wrist and arrived within
 * @ref ACTIVITY_HRM_OFFWRIST_STALE_SEC.
 */
bool activity_metrics_prv_is_hrm_offwrist(time_t now_utc);

/**
 * @brief Get the steps taken today.
 *
 * @return Steps.
 */
uint32_t activity_metrics_prv_get_steps(void);

/**
 * @brief Get the steps taken in the past minute.
 *
 * @return Steps.
 */
ActivityScalarStore activity_metrics_prv_steps_per_minute(void);

/**
 * @brief Set a metric's value, on behalf of the phone (health database).
 *
 * Today's value is only increased; past days of the last week are overwritten. Takes the
 * activity mutex.
 *
 * @param metric Metric.
 * @param day Day of the week of the value.
 * @param value Value, in activity_get_metric() units.
 */
void activity_metrics_prv_set_metric(ActivityMetric metric, enum pbl_weekday day, int32_t value);

/**
 * @brief Force today's value of a metric, possibly decreasing it (QEMU and test injection).
 *
 * Takes the activity mutex.
 *
 * @param metric Metric.
 * @param value Value, in activity_get_metric() units.
 */
void activity_metrics_set_metric_exact(ActivityMetric metric, int32_t value);

/** @} */
