/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "pbl/services/activity/activity.h"
#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_activity_activity_algorithm Activity algorithm interface
 * @ingroup services_activity
 * @brief Interface between the activity service and the step/sleep algorithm.
 *
 * The activity service calls these from KernelBG. The algorithm consumes accelerometer samples
 * at the rate returned by activity_algorithm_init(), keeps the running step count, builds one
 * minute record per minute, stores those in the minute file and sends them to data logging, and
 * reports detected sessions to the service.
 *
 * @code{.c}
 * AccelSamplingRate rate;
 *
 * activity_algorithm_init(&rate);
 * activity_algorithm_set_user(height_mm, weight_dag * 10, gender, age_years);
 *
 * // For every batch of accelerometer samples
 * activity_algorithm_handle_accel(samples, num_samples, timestamp_ms);
 *
 * // Once a minute
 * AlgMinuteRecord rec;
 * activity_algorithm_minute_handler(rtc_get_time(), &rec);
 *
 * uint32_t steps;
 * activity_algorithm_get_steps(&steps);
 * @endcode
 * @{
 */

/**
 * @brief Version of the minute file records.
 *
 * - 4: initial version;
 * - 5: added flags with the plugged_in and active bits;
 * - 6: added heart rate.
 */
#define ALG_MINUTE_FILE_RECORD_VERSION 6

/**
 * @brief Minute file sample, version 5 fields.
 *
 * The minute file is a settings file holding the subset of the data logging minute data needed
 * by the sleep algorithm and by minute history queries.
 */
typedef struct PBL_PACKED {
  // Base fields, present in versions 4 and 5
  /** Steps taken in this minute. */
  uint8_t steps;
  /**
   * Average orientation: upper 4 bits are the angle to the Z axis, lower 4 bits the angle in the
   * X-Y plane, each quantized to 16 steps.
   */
  uint8_t orientation;
  /** VMC (vector magnitude counts) of this minute. */
  uint16_t vmc;
  /** Light sensor reading divided by @ref ALG_RAW_LIGHT_SENSOR_DIVIDE_BY. */
  uint8_t light;
  // New fields added in version 5
  /** Minute flags. */
  union {
    /** Individual flags. */
    struct {
      /** Watch was plugged into a charger. */
      uint8_t plugged_in : 1;
      /** Active minute. */
      uint8_t active : 1;
      /** Reserved. */
      uint8_t reserved : 6;
    };
    /** All flags. */
    uint8_t flags;
  };
} AlgMinuteFileSampleV5;

/** @brief Minute file sample. */
typedef struct PBL_PACKED {
  /** Fields of versions up to 5. */
  AlgMinuteFileSampleV5 v5_fields;
  // New fields added in version 6
  /** Median heart rate, in beats per minute, 0 if none. */
  uint8_t heart_rate_bpm;
} AlgMinuteFileSample;

/**
 * @brief Version of the data logging minute records.
 *
 * Fields may only be appended: the mobile apps keep parsing the known prefix. Android 3.10-4.0
 * requires bit 2 to be set and iOS requires a value below 255, so valid versions are 4, 5, 6, 7,
 * 12, 13, 14, 15, 20, ...
 *
 * - 4: initial version;
 * - 5: added base flags;
 * - 6: added the active flag, resting and active calories, and distance;
 * - 7: added heart rate;
 * - 12: added total heart rate weight;
 * - 13: added heart rate zone;
 * - 14: added SpO2 percent and quality.
 */
#define ALG_DLS_MINUTES_RECORD_VERSION 14

_Static_assert((ALG_DLS_MINUTES_RECORD_VERSION & (1 << 2)) > 0,
               "Android 3.10-4.0 requires bit 2 to be set");
_Static_assert(ALG_DLS_MINUTES_RECORD_VERSION <= 225, "iOS requires version less that 255");

/** @brief Data logging minute sample. */
typedef struct PBL_PACKED {
  /** Base fields, also stored in the minute file (versions 4 and 5). */
  AlgMinuteFileSampleV5 base;

  // New fields added in version 6
  /** Resting calories (1/1000 kcal) burned in this minute. */
  uint16_t resting_calories;
  /** Active calories (1/1000 kcal) burned in this minute. */
  uint16_t active_calories;
  /** Distance covered in this minute, in centimeters. */
  uint16_t distance_cm;

  // New fields added in version 7
  /** Weighted median heart rate of this minute, in beats per minute. */
  uint8_t heart_rate_bpm;

  // New fields added in version 12
  /** Total weight of the heart rate samples, multiplied by 100. */
  uint16_t heart_rate_total_weight_x100;

  // New fields added in version 13
  /** Heart rate zone of this minute (@ref HRZone). */
  uint8_t heart_rate_zone;

  // New fields added in version 14
  /** Blood oxygen saturation (%) measured this minute, 0 if none. */
  uint8_t spo2_percent;
  /** SpO2 signal quality (@c HeartRateQuality value), 0 if none. */
  uint8_t spo2_quality;
} AlgMinuteDLSSample;

/**
 * @brief Minute record in the circular buffer.
 *
 * Records are batched there before being written to data logging and the minute file.
 */
typedef struct {
  /** UTC time of the minute. */
  time_t utc_sec;
  /** Minute data. */
  AlgMinuteDLSSample data;
} AlgMinuteRecord;

/** @brief Header of minute file and data logging minute records. */
typedef struct PBL_PACKED {
  /** @ref ALG_DLS_MINUTES_RECORD_VERSION or @ref ALG_MINUTE_FILE_RECORD_VERSION. */
  uint16_t version;
  /** UTC time of the first sample. */
  uint32_t time_utc;
  /** Number of 15 minute intervals to add to UTC to get local time. */
  int8_t time_local_offset_15_min;
  /** Size of each sample, in bytes. */
  uint8_t sample_size;
  /** Number of samples in the record. */
  uint8_t num_samples;
} AlgMinuteRecordHdr;

/** @brief Number of minutes in a data logging minute record. */
#define ALG_MINUTES_PER_DLS_RECORD 15
/** @brief Data logging minute record. */
typedef struct PBL_PACKED {
  /** Header. */
  AlgMinuteRecordHdr hdr;
  /** One sample per minute. */
  AlgMinuteDLSSample samples[ALG_MINUTES_PER_DLS_RECORD];
} AlgMinuteDLSRecord;

/** @brief Number of minutes in a minute file record. */
#define ALG_MINUTES_PER_FILE_RECORD 15
/** @brief Minute file record. */
typedef struct PBL_PACKED {
  /** Header. */
  AlgMinuteRecordHdr hdr;
  /** One sample per minute. */
  AlgMinuteFileSample samples[ALG_MINUTES_PER_FILE_RECORD];
} AlgMinuteFileRecord;

/** @brief Size quota of the minute file, in bytes. */
#define ALG_MINUTE_DATA_FILE_LEN 0x20000

/**
 * @brief Upper bound of the number of records in the minute file.
 *
 * Ignores the settings file overhead, so the actual number is lower.
 */
#define ALG_MINUTE_FILE_MAX_ENTRIES (ALG_MINUTE_DATA_FILE_LEN / sizeof(AlgMinuteFileRecord))

/**
 * @brief Initialize the algorithm.
 *
 * @param[out] sampling_rate Required accelerometer sampling rate.
 * @return true on success.
 */
bool activity_algorithm_init(AccelSamplingRate *sampling_rate);

/** @brief Start tearing down: end all ongoing sessions. */
void activity_algorithm_early_deinit(void);

/**
 * @brief Deinitialize the algorithm.
 *
 * @return true on success.
 */
bool activity_algorithm_deinit(void);

/**
 * @brief Set the user metrics used for the calorie computations.
 *
 * @param height_mm Height, in millimeters.
 * @param weight_g Weight, in grams.
 * @param gender Gender.
 * @param age_years Age, in years.
 * @return true on success.
 */
bool activity_algorithm_set_user(uint32_t height_mm, uint32_t weight_g, ActivityGender gender,
                                 uint32_t age_years);

/**
 * @brief Process accelerometer samples.
 *
 * @param data Samples, in mG.
 * @param num_samples Number of samples in @p data.
 * @param timestamp_ms Timestamp of the first sample, in milliseconds.
 */
void activity_algorithm_handle_accel(AccelRawData *data, uint32_t num_samples,
                                     uint64_t timestamp_ms);

/**
 * @brief Collect and log the stats of the last minute.
 *
 * Called once per minute. The minute data is mostly used to compute sleep.
 *
 * @param utc_sec UTC time at which the minute handler was triggered.
 * @param[out] record_out Minute record.
 */
void activity_algorithm_minute_handler(time_t utc_sec, AlgMinuteRecord *record_out);

/**
 * @brief Get the number of steps counted today.
 *
 * @param[out] steps Steps.
 * @return true on success.
 */
bool activity_algorithm_get_steps(uint32_t *steps);

/**
 * @brief Enable or disable automatic activity session detection.
 *
 * Disabled while a workout is running.
 *
 * @param enable true to detect sessions, false to stop.
 */
void activity_algorithm_enable_activity_tracking(bool enable);

/**
 * @brief Check whether a continuous heart rate session is active for a detected activity.
 *
 * Drives SpO2 sampling during activities.
 *
 * @return true if heart rate is being sampled for a detected walk or run.
 */
bool activity_algorithm_activity_hrm_is_active(void);

/**
 * @brief Pause or resume the heart rate session of a detected activity.
 *
 * Frees the optical path for a SpO2 reading. No-op without an activity heart rate session.
 *
 * @param paused true to pause, false to resume.
 */
void activity_algorithm_activity_hrm_set_paused(bool paused);

/**
 * @brief Get the most recent stepping rate.
 *
 * @param[out] steps Steps taken during @p elapsed_ms.
 * @param[out] elapsed_ms Duration over which the rate was computed, in milliseconds.
 * @param[out] end_sec UTC time at which the rate was last computed.
 * @return true on success.
 */
bool activity_algorithm_get_step_rate(uint16_t *steps, uint32_t *elapsed_ms, time_t *end_sec);

/**
 * @brief Notify the algorithm that the metrics changed.
 *
 * The algorithm reloads its running steps, calories and distance from the service. Called at
 * midnight to start a new day and whenever new values are written into the health
 * database.
 *
 * @return true on success.
 */
bool activity_algorithm_metrics_changed_notification(void);

/**
 * @brief Get the last minute processed by the sleep detector.
 *
 * @return UTC time of the minute.
 */
time_t activity_algorithm_get_last_sleep_utc(void);

/** @brief Send the buffered minute data to data logging now. */
void activity_algorithm_send_minutes(void);

/**
 * @brief Relabel sleep sessions that should be naps.
 *
 * A sleep session is a nap when it is at most @ref ALG_MAX_NAP_MINUTES long and lies entirely
 * between @ref ALG_PRIMARY_MORNING_MINUTE and @ref ALG_PRIMARY_EVENING_MINUTE.
 *
 * @param num_sessions Number of sessions in @p sessions.
 * @param[in,out] sessions Sessions.
 */
void activity_algorithm_post_process_sleep_sessions(uint16_t num_sessions,
                                                    ActivitySession *sessions);

/**
 * @brief Get historical minute data.
 *
 * @param[out] minute_data Array filled with one record per minute.
 * @param[in,out] num_records On entry, capacity of @p minute_data. On exit, number of records
 * written, missing minutes included (marked with @c is_invalid).
 * @param[in,out] utc_start On entry, UTC time of the first requested minute. On exit, UTC time
 * of the first returned minute.
 * @return true on success.
 */
bool activity_algorithm_get_minute_history(HealthMinuteData *minute_data, uint32_t *num_records,
                                           time_t *utc_start);

/**
 * @brief Dump the minute file base64-encoded with PBL_LOG.
 *
 * Used to extract it through a support request.
 *
 * @return true on success.
 */
bool activity_algorithm_dump_minute_data_to_log(void);

/**
 * @brief Get information on the minute file.
 *
 * @param compact_first If true, compact the file first.
 * @param[out] num_records Number of records in the file.
 * @param[out] data_bytes Bytes of data in the file.
 * @param[out] minutes Minutes of data in the file.
 * @return true on success.
 */
bool activity_algorithm_minute_file_info(bool compact_first, uint32_t *num_records,
                                         uint32_t *data_bytes, uint32_t *minutes);

/**
 * @brief Fill the minute file (test apps).
 *
 * @return true on success.
 */
bool activity_algorithm_test_fill_minute_file(void);

/**
 * @brief Send a fake minute record to data logging (mobile app testing).
 *
 * @return true on success.
 */
bool activity_algorithm_test_send_fake_minute_data_dls_record(void);

/** @} */
