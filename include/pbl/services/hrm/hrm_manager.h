/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/accel_manager_types.h>
#include <process_management/app_install_types.h>
#include <pbl/util/units.h>

#include <stdbool.h>
#include <stdint.h>
#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_hrm Heart rate monitor
 * @ingroup services
 * @brief Shared access to the optical heart rate sensor (BPM, HRV and SpO2).
 *
 * Subscribers ask for a set of features and an update interval. A subscriber is due when its
 * interval has elapsed since it last got a usable reading; the manager turns the sensor on
 * (@ref HRM_SENSOR_SPIN_UP_SEC early) while any subscriber is due and off otherwise. While on,
 * the driver reports data about once per second and every reading is delivered to every
 * subscriber of that feature, so intervals at or below the spin-up time keep the sensor on
 * continuously.
 *
 * The green (BPM, HRV) and red/IR (SpO2) optical paths are mutually exclusive; SpO2 wins when
 * due. Background subscribers only get the features allowed by the heart rate and blood oxygen
 * preferences, foreground apps get what they ask for. The sensor stays off while charging or
 * when the run level disables it.
 *
 * Apps and workers subscribe through the syscalls and receive @c PEBBLE_HRM_EVENT events on
 * their queue. KernelBG clients subscribe with a callback (see
 * @ref services_hrm_hrm_manager_private):
 *
 * @code{.c}
 * static void prv_hrm_cb(PebbleHRMEvent *event, void *context) {
 *   if (event->event_type == HRMEvent_BPM && event->bpm.quality >= HRMQuality_Acceptable) {
 *     // event->bpm.bpm
 *   } else if (event->event_type == HRMEvent_SubscriptionExpiring) {
 *     // renew with sys_hrm_manager_set_update_interval() if still needed
 *   }
 * }
 *
 * // 1 s updates, expiring after 10 minutes
 * HRMSessionRef session = hrm_manager_subscribe_with_callback(
 *     INSTALL_ID_INVALID, 1, 10 * 60, HRMFeature_BPM, false, prv_hrm_cb, NULL);
 * ...
 * sys_hrm_manager_unsubscribe(session);
 * @endcode
 * @{
 */

/** @brief Quality of a reading, as graded by the sensor algorithm. */
typedef enum {
  /** The watch is not on the wrist. */
  HRMQuality_OffWrist = -1,
  /** Worst quality. */
  HRMQuality_Worst = 0,
  /** Poor quality. */
  HRMQuality_Poor,
  /** Acceptable quality. */
  HRMQuality_Acceptable,
  /** Good quality. */
  HRMQuality_Good,
  /** Excellent quality. */
  HRMQuality_Excellent,
} HRMQuality;

/** @brief Bit positions of @ref HRMFeature. */
typedef enum {
  /** Heart rate. */
  HRMFeatureShift_BPM = 0,
  /** Heart rate variability. */
  HRMFeatureShift_HRV = 1,
  /** Blood oxygen saturation. */
  HRMFeatureShift_SpO2 = 2,
#ifdef CONFIG_MFG
  /** PPG CTR test data (manufacturing). */
  HRMFeatureShift_CTR = 3,
  /** PPG leakage test data (manufacturing). */
  HRMFeatureShift_Leakage = 4,
#endif
  /** Number of features. */
  HRMFeatureShiftMax
} HRMFeatureShift;

/** @brief Sensor features, combined as a bitmask. */
typedef enum {
  /** Heart rate, in beats per minute. */
  HRMFeature_BPM = (1 << HRMFeatureShift_BPM),
  /** Heart rate variability (peak-to-peak interval). */
  HRMFeature_HRV = (1 << HRMFeatureShift_HRV),
  /** Blood oxygen saturation. */
  HRMFeature_SpO2 = (1 << HRMFeatureShift_SpO2),
#ifdef CONFIG_MFG
  /** PPG CTR test data (manufacturing). */
  HRMFeature_CTR = (1 << HRMFeatureShift_CTR),
  /** PPG leakage test data (manufacturing). */
  HRMFeature_Leakage = (1 << HRMFeatureShift_Leakage),
#endif
  /** Upper bound of the feature values. */
  HRMFeatureMax
} HRMFeature;

/** @brief Accelerometer sampling rate provided to the HRM driver, in mHz (25 Hz). */
#define HRM_MANAGER_ACCEL_RATE_MILLIHZ (25000)
/** @brief Accelerometer samples buffered for the driver: 2 s, in case a handshake is missed. */
#define HRM_MANAGER_MAX_ACCEL_SAMPLES ((2 * HRM_MANAGER_ACCEL_RATE_MILLIHZ) / 1000)

/** @brief Expiration given to the subscription of an app that exits, in seconds. */
#define HRM_MANAGER_APP_EXIT_EXPIRATION_SEC PBL_SEC_PER_HOUR

/** @brief Accelerometer samples for the HRM driver's motion compensation. */
typedef struct {
  /** Samples, in mG. */
  AccelRawData data[HRM_MANAGER_MAX_ACCEL_SAMPLES];
  /** Number of valid samples in @ref data. */
  uint32_t num_samples;
} HRMAccelData;

/**
 * @brief Get the accelerometer samples collected since the last release.
 *
 * Locks the buffer until hrm_manager_release_accel_data(). For the HRM driver.
 *
 * @return Locked sample buffer.
 */
HRMAccelData *hrm_manager_get_accel_data(void);

/** @brief Empty and unlock the buffer returned by hrm_manager_get_accel_data(). */
void hrm_manager_release_accel_data(void);

/** @brief Subscription handle. */
typedef uint32_t HRMSessionRef;
/** @brief Invalid subscription handle. */
#define HRM_INVALID_SESSION_REF 0

/**
 * @brief Minimum warning before a subscription expires, in seconds.
 *
 * A @c HRMEvent_SubscriptionExpiring event is sent this long, or one update interval if longer,
 * before expiration.
 */
#define HRM_SUBSCRIPTION_EXPIRING_WARNING_SEC 5

/** @brief Initialize the HRM manager. */
void hrm_manager_init(void);

/** @brief Re-evaluate the sensor state after the HRM related preferences changed. */
void hrm_manager_handle_prefs_changed(void);

/**
 * @brief Check whether a live subscriber keeps the green optical path on continuously.
 *
 * That is, a BPM or HRV subscriber with an interval within @ref HRM_SENSOR_SPIN_UP_SEC (live
 * workout heart rate, the BLE heart rate relay, a foreground app). Background SpO2 readers use
 * it to defer their measurement window instead of taking the optical path away.
 *
 * @return true if such a subscriber exists.
 */
bool hrm_manager_has_continuous_green_subscriber(void);

/**
 * @brief Subscribe an app or worker to sensor updates.
 *
 * Not for KernelBG or KernelMain clients, see hrm_manager_subscribe_with_callback(). An existing
 * subscription of the caller is replaced, keeping its session reference. Events are delivered
 * as @c PEBBLE_HRM_EVENT to the caller's task. Foreground apps with an interval of at most
 * @ref HRM_LOW_LATENCY_MAX_INTERVAL_S get low latency updates.
 *
 * @param app_id Caller's app install id.
 * @param update_interval_s Requested update interval, in seconds.
 * @param expire_s Seconds after which the subscription expires, 0 for never.
 * @param features Requested features.
 * @return Session reference.
 */
HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features);

/**
 * @brief Get the subscription of an app or worker.
 *
 * Not for KernelBG subscriptions.
 *
 * @param app_id App install id.
 * @return Session reference, @ref HRM_INVALID_SESSION_REF if none.
 */
HRMSessionRef sys_hrm_manager_get_app_subscription(AppInstallId app_id);

/**
 * @brief Unsubscribe, turning the sensor off if no longer needed.
 *
 * @param session Session reference.
 * @return true on success, false if the subscription was not found.
 */
bool sys_hrm_manager_unsubscribe(HRMSessionRef session);

/**
 * @brief Change the features of a subscription.
 *
 * A subscription without features is kept but ignored: it never turns the sensor on and
 * receives no data.
 *
 * @param session Session reference.
 * @param features Requested features.
 * @return true on success, false if the subscription was not found.
 */
bool sys_hrm_manager_set_features(HRMSessionRef session, HRMFeature features);

/**
 * @brief Change the update interval and expiration of a subscription.
 *
 * @param session Session reference.
 * @param update_interval_s Requested update interval, in seconds.
 * @param expire_s Seconds from now after which the subscription expires, 0 for never.
 * @return true on success, false if the subscription was not found.
 */
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s);

/**
 * @brief Get information on a subscription.
 *
 * @param session Session reference.
 * @param[out] app_id If not NULL, app install id of the subscriber.
 * @param[out] update_interval_s If not NULL, update interval, in seconds.
 * @param[out] expire_s If not NULL, seconds until expiration, 0 for never.
 * @param[out] features If not NULL, requested features.
 * @return true on success, false if the subscription was not found.
 */
bool sys_hrm_manager_get_subscription_info(HRMSessionRef session, AppInstallId *app_id,
                                           uint32_t *update_interval_s, uint16_t *expire_s,
                                           HRMFeature *features);

/**
 * @brief Check whether a heart rate monitor is present and accessible.
 *
 * @return true if the HRM service is built in (@c CONFIG_SERVICE_HRM).
 */
bool sys_hrm_manager_is_hrm_present(void);

/**
 * @brief Enable or disable the HRM manager for the current run level.
 *
 * Subscribers are kept while disabled but get no updates.
 *
 * @param on Whether the sensor may be turned on.
 */
void hrm_manager_enable(bool on);

//------------------------------------------------------------------------------
// HRM Driver Interface
//------------------------------------------------------------------------------
// The driver needs to provide new data to the service and needs to pull accel data.

/** @brief Data reported by the HRM driver. */
typedef struct {
  /** Features with valid data in this report. */
  HRMFeature features;

  /** Heart rate, in beats per minute. */
  uint8_t hrm_bpm;
  /** Heart rate quality. */
  HRMQuality hrm_quality;

  /** Peak-to-peak interval, in milliseconds. */
  uint16_t hrv_ppi_ms;
  /** HRV quality. */
  HRMQuality hrv_quality;

  /** Blood oxygen saturation, in percent. */
  uint8_t spo2_percent;
  /** SpO2 quality. */
  HRMQuality spo2_quality;
  /** Raw algorithm confidence coefficient (debug). */
  uint8_t spo2_confidence;
  /** Raw algorithm valid level (debug). */
  uint8_t spo2_valid_level;
  /** Raw algorithm invalid flag (debug); a reading with it clear is usable. */
  bool spo2_invalid;

#ifdef CONFIG_MFG
  /** PPG CTR test data, per channel (manufacturing). */
  double ctr[6];
  /** PPG leakage test data, per channel (manufacturing). */
  double leakage[6];
#endif
} HRMData;

/**
 * @brief Report new data from the HRM driver.
 *
 * Called about once per second while the sensor is on. Queues events for the subscribers of the
 * reported features.
 *
 * @param data New data.
 */
void hrm_manager_new_data_cb(const HRMData *data);

/**
 * @brief Clean up after an app or worker exited.
 *
 * Makes its subscription expire within @ref HRM_MANAGER_APP_EXIT_EXPIRATION_SEC, unless it
 * already expires sooner.
 *
 * @param task Task of the process.
 * @param app_id App install id.
 */
void hrm_manager_process_cleanup(PebbleTask task, AppInstallId app_id);

/** @} */
