/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/ppg.h>
#include <pbl/services/hrm/hrm_activity_scene.h>
#include <pbl/services/hrm/hrm_manager.h>

/**
 * @defgroup services_hrm_ppg HRM signal processing
 * @ingroup services_hrm
 * @brief Runs the PPG front end for the HRM manager and feeds an algorithm backend.
 *
 * Each role is served by a slot of the front end, chosen by the LED it lights. The service runs one
 * optical path at a time: the green path for heart rate and HRV, with wear detection, or the red
 * and infrared path for SpO2, during which the watch is assumed worn. It keeps every AGC-controlled
 * channel in range, groups the samples of a sampling period into a frame, pairs it with the latest
 * accelerometer sample and hands it to the backend, which reports results back with
 * hrm_ppg_report_*().
 * @{
 */

/** @brief Maximum number of channels in a frame. */
#define HRM_PPG_MAX_CHANNELS 6

/** @brief Purpose of a front end channel. */
enum hrm_ppg_role {
  /** Green, for heart rate and HRV. */
  HRM_PPG_ROLE_GREEN,
  /** Red, for SpO2. */
  HRM_PPG_ROLE_RED,
  /** Infrared, for SpO2. */
  HRM_PPG_ROLE_IR,
  /** Infrared, for wear detection. */
  HRM_PPG_ROLE_WEAR,
  /** Ambient light. */
  HRM_PPG_ROLE_AMBIENT,
  /** Number of roles. */
  HRM_PPG_NUM_ROLES,
};

/** @brief AGC target window. */
enum hrm_ppg_agc_target {
  /** Heart rate and wear detection signals. */
  HRM_PPG_AGC_GENERAL,
  /** SpO2 signals, kept higher in the receiver range. */
  HRM_PPG_AGC_SPO2,
};

/** @brief A channel of a frame. */
struct hrm_ppg_channel {
  /** Role. */
  enum hrm_ppg_role role;
  /** Receiver. */
  uint8_t rx;
};

/** @brief Session started on the backend. */
struct hrm_ppg_session {
  /** Features requested. */
  HRMFeature features;
  /** Sample rate, in hertz. */
  uint16_t sample_rate_hz;
  /** Channels of every frame, in order. */
  struct hrm_ppg_channel channels[HRM_PPG_MAX_CHANNELS];
  /** Number of channels. */
  uint8_t num_channels;
  /** Front end description. */
  const struct pbl_ppg_info *info;
};

/** @brief One sampling period of the session channels. */
struct hrm_ppg_frame {
  /** Sample values, as reported by the front end. */
  int32_t value[HRM_PPG_MAX_CHANNELS];
  /** LED current the sample was taken with, in microamperes. */
  uint32_t led_ua[HRM_PPG_MAX_CHANNELS];
  /** Receiver gain the sample was taken with, in ohms. */
  uint32_t gain_ohm[HRM_PPG_MAX_CHANNELS];
  /** Acceleration, in milli-g. */
  int16_t accel_mg[3];
};

/** @brief Algorithm backend. Calls come from the system work queue, except set_scene(). */
struct hrm_ppg_backend {
  /** Start a session. Returns 0 or a negative errno. */
  int (*start)(const struct hrm_ppg_session *session);
  /** Stop the session. */
  void (*stop)(void);
  /** Process a frame. */
  void (*process)(const struct hrm_ppg_frame *frame);
  /** Optional. Set the activity in progress, from any task, at any time. */
  void (*set_scene)(HRMActivityScene scene);
};

/** @brief The algorithm backend. */
extern const struct hrm_ppg_backend HRM_PPG_BACKEND;

/**
 * @brief Report a heart rate.
 *
 * @param bpm Heart rate, in beats per minute.
 * @param confidence Confidence, 0 to 100.
 */
void hrm_ppg_report_bpm(uint8_t bpm, uint8_t confidence);

/**
 * @brief Report an SpO2 reading.
 *
 * @param percent Oxygen saturation, in percent.
 * @param confidence Confidence, 0 to 100.
 * @param valid_level Algorithm valid level, for debugging.
 * @param invalid True if the algorithm rejected the reading.
 */
void hrm_ppg_report_spo2(uint8_t percent, uint8_t confidence, uint8_t valid_level, bool invalid);

/**
 * @brief Report beat-to-beat intervals.
 *
 * @param rri_ms Intervals, in milliseconds.
 * @param num Number of intervals.
 * @param confidence Confidence, 0 to 100.
 */
void hrm_ppg_report_hrv(const int32_t *rri_ms, uint8_t num, uint8_t confidence);

/**
 * @brief Report a wear verdict.
 *
 * @param worn True if the watch is on the wrist.
 */
void hrm_ppg_report_wear(bool worn);

/** @cond INTERNAL_HIDDEN */
int hrm_ppg_start(HRMFeature features, bool low_latency);
void hrm_ppg_stop(void);
bool hrm_ppg_is_running(void);
void hrm_ppg_set_scene(HRMActivityScene scene);
/** @endcond */

/** @} */
