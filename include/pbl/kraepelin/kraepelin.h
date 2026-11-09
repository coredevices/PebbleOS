#pragma once
#include <stdint.h>
#include <time.h>

/**
 * @defgroup kraepelin Kraepelin
 * @ingroup lib
 * @brief Step counting, sleep and activity session detection.
 *
 * The algorithm processes accelerometer samples at @ref PBL_KRAEPELIN_SAMPLE_HZ in 5 second epochs
 * (125 samples). For each epoch it computes the FFT and the VMC (vector magnitude counts, a measure
 * of movement calibrated against the Actigraph) and identifies the stepping frequency, giving
 * the steps of the epoch. Once a minute, the minute stats (VMC, orientation) are fed with the
 * minute's steps, calories and distance to state machines that detect walks, runs, sleep and
 * restful sleep. See the health algorithms page of the architecture documentation for details.
 *
 * The caller owns the state, of size pbl_kraepelin_state_size():
 *
 * @code{.c}
 * struct pbl_kraepelin *state = malloc(pbl_kraepelin_state_size());
 * pbl_kraepelin_init(state, nullptr, nullptr);
 *
 * // For every batch of 25 Hz samples
 * uint32_t consumed;
 * steps += pbl_kraepelin_analyze_samples(state, samples, num_samples, &consumed);
 *
 * // Once a minute
 * uint16_t vmc;
 * uint8_t orientation;
 * bool still;
 * pbl_kraepelin_minute_stats(state, &vmc, &orientation, &still);
 * pbl_kraepelin_activities_update(state, time(NULL), minute_steps, vmc, orientation, not_worn,
 *                        resting_cal, active_cal, distance_mm, false, session_cb, ctx);
 *
 * free(state);
 * @endcode
 * @{
 */

/** @brief Accelerometer sampling rate expected by the algorithm, in Hz. */
#define PBL_KRAEPELIN_SAMPLE_HZ 25

/** @brief Accelerometer sample, in mG. */
struct pbl_kraepelin_sample {
  int16_t x;
  int16_t y;
  int16_t z;
};

/** @brief Opaque algorithm state. */
struct pbl_kraepelin;

/**
 * @brief Maximum delay, in minutes, for the sleep algorithm to detect that the user woke up.
 *
 * Needed at compile time by the activity algorithm glue, so it is hard-coded; pbl_kraepelin_init()
 * asserts it matches the sleep parameters.
 */
#define PBL_KRAEPELIN_MAX_UNCERTAIN_SLEEP_M 19

/** @brief Activity types reported to @ref pbl_kraepelin_session_cb_t. */
enum pbl_kraepelin_activity {
  /**
   * Entire sleep session from falling asleep to waking up, containing both light and restful
   * periods.
   */
  PBL_KRAEPELIN_ACTIVITY_SLEEP,

  /** Restful period, always inside a @ref PBL_KRAEPELIN_ACTIVITY_SLEEP session. */
  PBL_KRAEPELIN_ACTIVITY_RESTFUL_SLEEP,

  /** Walk of significant length. */
  PBL_KRAEPELIN_ACTIVITY_WALK,

  /** Run. */
  PBL_KRAEPELIN_ACTIVITY_RUN,

  // Leave at end
  /** Number of activity types. */
  PBL_KRAEPELIN_ACTIVITY_COUNT,
};

/** @brief Ongoing sleep statistics, returned by pbl_kraepelin_get_sleep_stats(). */
struct pbl_kraepelin_sleep_stats {
  /**
   * Start time of a recent sleep session, UTC; 0 if none was detected in the last 60 minutes
   * (minimum sleep session length).
   */
  time_t sleep_start_utc;
  /** Minutes of that sleep that are certain, 0 if none. */
  uint16_t sleep_len_m;
  /** Start of the uncertain part of the session, which lasts until now, UTC; 0 if none. */
  time_t uncertain_start_utc;
};

/** @brief Walk or run in progress, returned by pbl_kraepelin_get_step_activity(). */
struct pbl_kraepelin_step_activity {
  /** Start time, UTC; 0 if no activity is in progress. */
  time_t start_utc;
  /** Time since the start as of the last minute processed, in seconds. */
  uint32_t len_sec;
  /** true if the last minute processed was an active minute of the activity. */
  bool active;
};

/**
 * @brief Callback reporting activity sessions, called by pbl_kraepelin_activities_update().
 *
 * A session may be reported several times while ongoing, and deleted later.
 *
 * @param context Context passed to pbl_kraepelin_activities_update().
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
typedef void (*pbl_kraepelin_session_cb_t)(void *context, enum pbl_kraepelin_activity activity_type,
                                           time_t start_utc, uint32_t len_sec, bool ongoing,
                                           bool delete, uint32_t steps, uint32_t resting_calories,
                                           uint32_t active_calories, uint32_t distance_mm);

/**
 * @brief Detection parameters that depend on the accelerometer, passed to pbl_kraepelin_init().
 *
 * Accelerometers with a higher noise floor need higher VMC thresholds.
 */
struct pbl_kraepelin_params {
  /** A minute whose weighted VMC score is at most this is a sleep minute. */
  uint16_t sleep_max_minute_score;
  /** A minute whose VMC is above this counts as an active minute of a sleep session. */
  uint16_t sleep_min_valid_vmc;
  /** A sleep session whose average VMC is above this is rejected. */
  uint16_t sleep_max_avg_vmc;
  /** A minute whose VMC is below this may be a not-worn minute. */
  uint16_t not_worn_min_worn_vmc;
  /** A run of possibly not-worn minutes at least this long, in minutes, is not worn. */
  uint16_t not_worn_max_low_vmc_run_m;
};

/** @brief Default @ref pbl_kraepelin_params initializer. */
#define PBL_KRAEPELIN_PARAMS_DEFAULT   \
  {                                    \
    .sleep_max_minute_score = 330,     \
    .sleep_min_valid_vmc = 20,         \
    .sleep_max_avg_vmc = 180,          \
    .not_worn_min_worn_vmc = 4,        \
    .not_worn_max_low_vmc_run_m = 180, \
  }

/**
 * @brief Callback receiving algorithm statistics.
 *
 * Only used during algorithm development, to collect and summarize named statistics.
 *
 * @param num_stats Number of elements in @p names and @p stats.
 * @param names Statistic names.
 * @param stats Statistic values.
 */
typedef void (*pbl_kraepelin_stats_cb_t)(uint32_t num_stats, const char **names, int32_t *stats);

/**
 * @brief Get the size of the algorithm state.
 *
 * @return Size of the state, in bytes.
 */
uint32_t pbl_kraepelin_state_size(void);

/**
 * @brief Initialize the algorithm state.
 *
 * @param state State, of pbl_kraepelin_state_size() bytes.
 * @param params Detection parameters, copied; NULL for @ref PBL_KRAEPELIN_PARAMS_DEFAULT.
 * @param stats_cb If not NULL, called with statistics while analyzing samples.
 * @return true on success.
 */
bool pbl_kraepelin_init(struct pbl_kraepelin *state, const struct pbl_kraepelin_params *params,
                        pbl_kraepelin_stats_cb_t stats_cb);

/**
 * @brief Analyze accelerometer samples.
 *
 * Samples are buffered until a full epoch is available.
 *
 * @param state State passed to pbl_kraepelin_init().
 * @param samples Samples, in mG, at @ref PBL_KRAEPELIN_SAMPLE_HZ.
 * @param num_samples Number of samples in @p samples.
 * @param[out] consumed_samples Number of samples just processed to compute steps: an epoch
 * (125) when one completed, 0 otherwise.
 * @return Steps counted.
 */
uint32_t pbl_kraepelin_analyze_samples(struct pbl_kraepelin *state,
                                       const struct pbl_kraepelin_sample *samples,
                                       uint32_t num_samples, uint32_t *consumed_samples);

/**
 * @brief Get the last minute's stats and reset them for the next minute.
 *
 * The minute stats are logged and used to compute sleep.
 *
 * @param state State passed to pbl_kraepelin_init().
 * @param[out] vmc VMC (vector magnitude counts) of the minute.
 * @param[out] orientation Average orientation: upper 4 bits are the angle to the Z axis, lower
 * 4 bits the angle in the X-Y plane, each quantized to 16 steps.
 * @param[out] still true if no movement above the noise threshold was detected (currently
 * always false).
 */
void pbl_kraepelin_minute_stats(struct pbl_kraepelin *state, uint16_t *vmc, uint8_t *orientation,
                                bool *still);

/**
 * @brief Process the partial epoch not yet processed by pbl_kraepelin_analyze_samples() (unit
 * tests).
 *
 * @param state State passed to pbl_kraepelin_init().
 * @return Steps counted.
 */
uint32_t pbl_kraepelin_analyze_finish_epoch(struct pbl_kraepelin *state);

/**
 * @brief Feed a minute of data to the walk, run and sleep detectors.
 *
 * Does nothing while activity tracking is disabled. A UTC time jump (backwards, or more than 5
 * minutes forward) resets the detectors.
 *
 * @param state State passed to pbl_kraepelin_init().
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
void pbl_kraepelin_activities_update(struct pbl_kraepelin *state, time_t utc_now, uint16_t steps,
                                     uint16_t vmc, uint8_t orientation, bool definitely_not_worn,
                                     uint32_t resting_calories, uint32_t active_calories,
                                     uint32_t distance_mm, bool shutting_down,
                                     pbl_kraepelin_session_cb_t sessions_cb, void *context);

/**
 * @brief Get the last minute processed for an activity type.
 *
 * @param state State passed to pbl_kraepelin_init().
 * @param activity Activity type.
 * @return UTC time of the minute.
 */
time_t pbl_kraepelin_activity_last_processed_time(struct pbl_kraepelin *state,
                                                  enum pbl_kraepelin_activity activity);

/**
 * @brief Get the walk or run in progress.
 *
 * An activity is in progress from its first active minute, before it is long enough to be
 * reported to @ref pbl_kraepelin_session_cb_t, until it ends.
 *
 * @param state State passed to pbl_kraepelin_init().
 * @param activity @ref PBL_KRAEPELIN_ACTIVITY_WALK or @ref PBL_KRAEPELIN_ACTIVITY_RUN.
 * @param[out] step_activity Activity in progress.
 */
void pbl_kraepelin_get_step_activity(struct pbl_kraepelin *state,
                                     enum pbl_kraepelin_activity activity,
                                     struct pbl_kraepelin_step_activity *step_activity);

/**
 * @brief Get the ongoing sleep statistics.
 *
 * @param state State passed to pbl_kraepelin_init().
 * @param[out] stats Sleep statistics.
 */
void pbl_kraepelin_get_sleep_stats(struct pbl_kraepelin *state,
                                   struct pbl_kraepelin_sleep_stats *stats);

/**
 * @brief Enable or disable automatic activity session detection.
 *
 * Ends the step activities in progress and resets the detectors.
 *
 * @param kalg_state State passed to pbl_kraepelin_init().
 * @param enable true to detect sessions, false to stop.
 * @param sessions_cb Called for each step activity ended.
 * @param context Passed to @p sessions_cb.
 */
void pbl_kraepelin_enable_activity_tracking(struct pbl_kraepelin *kalg_state, bool enable,
                                            pbl_kraepelin_session_cb_t sessions_cb, void *context);

/** @} */
