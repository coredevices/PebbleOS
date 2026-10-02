#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @defgroup services_activity_kraepelin Kraepelin algorithm
 * @ingroup services_activity
 * @brief Step counting, sleep and activity session detection.
 *
 * The algorithm processes accelerometer samples at @ref KALG_SAMPLE_HZ in 5 second epochs (125
 * samples). For each epoch it computes the FFT and the VMC (vector magnitude counts, a measure
 * of movement calibrated against the Actigraph) and identifies the stepping frequency, giving
 * the steps of the epoch. Once a minute, the minute stats (VMC, orientation) are fed with the
 * minute's steps, calories and distance to state machines that detect walks, runs, sleep and
 * restful sleep. See the health algorithms page of the architecture documentation for details.
 *
 * The caller owns the state, of size kalg_state_size():
 *
 * @code{.c}
 * KAlgState *state = kernel_malloc_check(kalg_state_size());
 * kalg_init(state, NULL);
 *
 * // For every batch of 25 Hz samples
 * uint32_t consumed;
 * steps += kalg_analyze_samples(state, samples, num_samples, &consumed);
 *
 * // Once a minute
 * uint16_t vmc;
 * uint8_t orientation;
 * bool still;
 * kalg_minute_stats(state, &vmc, &orientation, &still);
 * kalg_activities_update(state, rtc_get_time(), minute_steps, vmc, orientation, not_worn,
 *                        resting_cal, active_cal, distance_mm, false, session_cb, ctx);
 *
 * kalg_deinit(state);
 * kernel_free(state);
 * @endcode
 * @{
 */

/** @brief Accelerometer sampling rate expected by the algorithm, in Hz. */
#define KALG_SAMPLE_HZ 25

/** @brief Number of grams per kilogram. */
#define KALG_GRAMS_PER_KG 1000

/** @brief Opaque algorithm state. */
typedef struct KAlgState KAlgState;

/** @brief Encoded VMC value meaning the watch was not worn. */
#define KALG_ENCODED_VMC_NOT_WORN 0

/** @brief Minimum encoded VMC value when the watch was worn. */
#define KALG_ENCODED_VMC_MIN_WORN_VALUE 1

/**
 * @brief Maximum delay, in minutes, for the sleep algorithm to detect that the user woke up.
 *
 * Needed at compile time by the activity algorithm glue, so it is hard-coded; kalg_init()
 * asserts it matches the sleep parameters.
 */
#define KALG_MAX_UNCERTAIN_SLEEP_M 19

/** @brief Activity types reported to @ref KAlgActivitySessionCallback. */
typedef enum {
  /**
   * Entire sleep session from falling asleep to waking up, containing both light and restful
   * periods.
   */
  KAlgActivityType_Sleep,

  /** Restful period, always inside a @ref KAlgActivityType_Sleep session. */
  KAlgActivityType_RestfulSleep,

  /** Walk of significant length. */
  KAlgActivityType_Walk,

  /** Run. */
  KAlgActivityType_Run,

  // Leave at end
  /** Number of activity types. */
  KAlgActivityTypeCount,
} KAlgActivityType;

/** @brief Ongoing sleep statistics, returned by kalg_get_sleep_stats(). */
typedef struct {
  /**
   * Start time of a recent sleep session, UTC; 0 if none was detected in the last 60 minutes
   * (minimum sleep session length).
   */
  time_t sleep_start_utc;
  /** Minutes of that sleep that are certain, 0 if none. */
  uint16_t sleep_len_m;
  /** Start of the uncertain part of the session, which lasts until now, UTC; 0 if none. */
  time_t uncertain_start_utc;
} KAlgOngoingSleepStats;

/**
 * @brief Callback reporting activity sessions, called by kalg_activities_update().
 *
 * A session may be reported several times while ongoing, and deleted later.
 *
 * @param context Context passed to kalg_activities_update().
 * @param activity_type Activity type.
 * @param start_utc Start time, UTC.
 * @param len_sec Length, in seconds.
 * @param ongoing true if the activity is still ongoing.
 * @param delete true to delete this previously reported session.
 * @param steps Steps taken.
 * @param resting_calories Resting calories (1/1000 kcal) burned.
 * @param active_calories Active calories (1/1000 kcal) burned.
 * @param distance_mm Distance covered, in millimeters.
 */
typedef void (*KAlgActivitySessionCallback)(void *context, KAlgActivityType activity_type,
                                            time_t start_utc, uint32_t len_sec, bool ongoing,
                                            bool delete, uint32_t steps, uint32_t resting_calories,
                                            uint32_t active_calories, uint32_t distance_mm);

/**
 * @brief Callback receiving algorithm statistics.
 *
 * Only used during algorithm development, to collect and summarize named statistics.
 *
 * @param num_stats Number of elements in @p names and @p stats.
 * @param names Statistic names.
 * @param stats Statistic values.
 */
typedef void (*KAlgStatsCallback)(uint32_t num_stats, const char **names, int32_t *stats);

/**
 * @brief Get the size of the algorithm state.
 *
 * @return Size of @ref KAlgState, in bytes.
 */
uint32_t kalg_state_size(void);

/**
 * @brief Initialize the algorithm state.
 *
 * @param state State, of kalg_state_size() bytes.
 * @param stats_cb If not NULL, called with statistics while analyzing samples.
 * @return true on success.
 */
bool kalg_init(KAlgState *state, KAlgStatsCallback stats_cb);

/**
 * @brief Release the resources held by the state.
 *
 * Must be called before freeing the state, otherwise the activity heart rate subscriptions
 * outlive it.
 *
 * @param state State passed to kalg_init().
 */
void kalg_deinit(KAlgState *state);

/**
 * @brief Analyze accelerometer samples.
 *
 * Samples are buffered until a full epoch is available.
 *
 * @param state State passed to kalg_init().
 * @param samples Samples, in mG, at @ref KALG_SAMPLE_HZ.
 * @param num_samples Number of samples in @p samples.
 * @param[out] consumed_samples Number of samples just processed to compute steps: an epoch
 * (125) when one completed, 0 otherwise.
 * @return Steps counted.
 */
uint32_t kalg_analyze_samples(KAlgState *state, AccelRawData *samples, uint32_t num_samples,
                              uint32_t *consumed_samples);

/**
 * @brief Get the last minute's stats and reset them for the next minute.
 *
 * The minute stats are logged and used to compute sleep.
 *
 * @param state State passed to kalg_init().
 * @param[out] vmc VMC (vector magnitude counts) of the minute.
 * @param[out] orientation Average orientation: upper 4 bits are the angle to the Z axis, lower
 * 4 bits the angle in the X-Y plane, each quantized to 16 steps.
 * @param[out] still true if no movement above the noise threshold was detected (currently
 * always false).
 */
void kalg_minute_stats(KAlgState *state, uint16_t *vmc, uint8_t *orientation, bool *still);

/**
 * @brief Process the partial epoch not yet processed by kalg_analyze_samples() (unit tests).
 *
 * @param state State passed to kalg_init().
 * @return Steps counted.
 */
uint32_t kalg_analyze_finish_epoch(KAlgState *state);

/**
 * @brief Feed a minute of data to the walk, run and sleep detectors.
 *
 * Does nothing while activity tracking is disabled. A UTC time jump (backwards, or more than 5
 * minutes forward) resets the detectors.
 *
 * @param state State passed to kalg_init().
 * @param utc_now Current UTC time.
 * @param steps Steps taken in the last minute.
 * @param vmc VMC of the last minute.
 * @param orientation Average orientation of the last minute.
 * @param definitely_not_worn true if the watch is definitely not worn this minute (on the
 * charger, or a recent off-wrist heart rate reading); a hard not-worn signal for sleep detection.
 * @param resting_calories Resting calories (1/1000 kcal) burned in the last minute.
 * @param active_calories Active calories (1/1000 kcal) burned in the last minute.
 * @param distance_mm Distance covered in the last minute, in millimeters.
 * @param shutting_down true to force all ongoing activities to end.
 * @param sessions_cb Called for every session found.
 * @param context Passed to @p sessions_cb.
 */
void kalg_activities_update(KAlgState *state, time_t utc_now, uint16_t steps, uint16_t vmc,
                            uint8_t orientation, bool definitely_not_worn,
                            uint32_t resting_calories, uint32_t active_calories,
                            uint32_t distance_mm, bool shutting_down,
                            KAlgActivitySessionCallback sessions_cb, void *context);

/**
 * @brief Get the last minute processed for an activity type.
 *
 * @param state State passed to kalg_init().
 * @param activity Activity type.
 * @return UTC time of the minute.
 */
time_t kalg_activity_last_processed_time(KAlgState *state, KAlgActivityType activity);

/**
 * @brief Get the ongoing sleep statistics.
 *
 * @param state State passed to kalg_init().
 * @param[out] stats Sleep statistics.
 */
void kalg_get_sleep_stats(KAlgState *state, KAlgOngoingSleepStats *stats);

/**
 * @brief Enable or disable automatic activity session detection.
 *
 * Resets the detectors.
 *
 * @param kalg_state State passed to kalg_init().
 * @param enable true to detect sessions, false to stop.
 */
void kalg_enable_activity_tracking(KAlgState *kalg_state, bool enable);

/**
 * @brief Check whether a continuous heart rate session is active for a detected activity.
 *
 * @param kalg_state State passed to kalg_init().
 * @return true if a walk or run heart rate session is active.
 */
bool kalg_activity_hrm_is_active(KAlgState *kalg_state);

/**
 * @brief Pause or resume the heart rate sessions of detected activities.
 *
 * Pausing sets their update interval to a day, freeing the shared optical path for a SpO2
 * reading; resuming restores the 1 second interval.
 *
 * @param kalg_state State passed to kalg_init().
 * @param paused true to pause, false to resume.
 */
void kalg_activity_hrm_set_paused(KAlgState *kalg_state, bool paused);

/** @} */
