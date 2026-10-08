/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/services/imu/units.h>

/**
 * @defgroup drivers_accel Accelerometer
 * @ingroup drivers
 * @brief Accelerometer driver interface.
 *
 * The driver is a thin layer over the hardware: it keeps no sample buffers and knows nothing
 * about clients, tasks or subsampling. That is left to the accelerometer service, so that code
 * shared by all drivers lives in one place. The interface is made of functions the driver
 * implements and callbacks (@c accel_cb_*, @c accel_offload_*) the service implements.
 *
 * Hardware state such as FIFO modes is hidden: the service only states its requirements
 * (sampling interval, batch size) and the driver picks the hardware configuration.
 *
 * @code{.c}
 * uint32_t interval_us = accel_set_sampling_interval(40000);
 * accel_set_num_samples(MIN(25, accel_get_max_num_samples()));
 *
 * void accel_cb_new_sample(AccelDriverSample const *data) {
 *   process(data->timestamp_us, data->x, data->y, data->z);
 * }
 * @endcode
 * @{
 */

/** @brief Accelerometer sample. */
typedef struct {
  /** Sample time in microseconds since the epoch, with no guaranteed precision. */
  uint64_t timestamp_us;
  /** Acceleration along the x axis, in milli-g. */
  int16_t x;
  /** Acceleration along the y axis, in milli-g. */
  int16_t y;
  /** Acceleration along the z axis, in milli-g. */
  int16_t z;
} AccelDriverSample;

/**
 * @brief Batch of raw samples in a driver-owned buffer.
 *
 * Handed to accel_cb_new_samples() so the service can scale and copy the samples in one pass
 * instead of taking a callback per sample.
 */
typedef struct {
  /** First raw sample, past any per-sample header. */
  const uint8_t *data;
  /** Number of samples in the buffer. */
  uint16_t num_samples;
  /** Bytes between consecutive samples, which lets the service skip e.g. FIFO tags. */
  uint8_t stride;
  /** Per-axis source, indexed by IMUCoordinateAxis; encodes axis remapping and direction. */
  struct {
    /** Byte offset of the little-endian int16 value within a sample. */
    uint8_t offset;
    /** Sign applied to the value, +1 or -1. */
    int8_t sign;
  } axis[3];
  /** Raw-count to milli-g numerator: mg = raw * scale_num / scale_den. */
  int32_t scale_num;
  /** Raw-count to milli-g denominator. */
  int32_t scale_den;
  /** Timestamp of the first sample, in microseconds since the epoch. */
  uint64_t first_timestamp_us;
  /** Interval between samples, in microseconds. */
  uint32_t sampling_interval_us;
} AccelRawBatch;

/** @brief Initialize the accelerometer. */
void accel_init(void);

/**
 * @brief Set whether the axes are rotated by 180 degrees.
 *
 * @param rotated True if the sensor is mounted rotated by 180 degrees.
 */
void accel_set_rotated(bool rotated);

/**
 * @brief Set the sampling interval.
 *
 * The driver selects the longest supported interval that is equal to or shorter than the
 * requested one, saturating at the shortest interval the hardware supports. The new interval
 * takes effect immediately; the driver may flush queued samples first so that timestamps stay
 * accurate.
 *
 * @param interval_us Requested sampling interval in microseconds.
 * @return Sampling interval actually used, in microseconds.
 */
uint32_t accel_set_sampling_interval(uint32_t interval_us);

/**
 * @brief Get the sampling interval.
 *
 * @return Current sampling interval in microseconds.
 */
uint32_t accel_get_sampling_interval(void);

/**
 * @brief Set the maximum number of samples the driver may batch.
 *
 * - 0: the driver must not call accel_cb_new_sample().
 * - 1: the driver calls accel_cb_new_sample() for every sample as soon as it is acquired.
 * - n > 1: the driver may queue up to n samples and then deliver them in rapid succession,
 *   the last one being the most recently acquired. This is only an upper bound, which the
 *   driver can use for power saving.
 *
 * When n is lowered below the number of samples already queued, the driver flushes them
 * before the new value takes effect, possibly from within this call.
 *
 * @param num_samples Maximum number of samples to batch, at most accel_get_max_num_samples().
 */
void accel_set_num_samples(uint32_t num_samples);

/**
 * @brief Get the maximum number of samples the driver can batch.
 *
 * @return Depth of the hardware FIFO, the upper bound for accel_set_num_samples().
 */
uint32_t accel_get_max_num_samples(void);

/**
 * @brief Read the most recent sample.
 *
 * The driver may call accel_cb_new_sample() from within this function if batching is enabled
 * (accel_set_num_samples() was last called with a non-zero value).
 *
 * @param[out] data Most recent sample.
 * @retval 0 Success.
 * @retval nonzero Failure.
 */
int accel_peek(AccelDriverSample *data);

/**
 * @brief Enable or disable shake detection.
 *
 * While enabled, the driver calls accel_cb_shake_detected() for every detected shake; while
 * disabled it does not.
 *
 * @param on True to enable, false to disable.
 */
void accel_enable_shake_detection(bool on);

/**
 * @brief Check whether shake detection is enabled.
 *
 * @return True if shake detection is enabled.
 */
bool accel_get_shake_detection_enabled(void);

/**
 * @brief Enable or disable double tap detection.
 *
 * While enabled, the driver calls accel_cb_double_tap_detected() for every detected double
 * tap; while disabled it does not.
 *
 * @param on True to enable, false to disable.
 */
void accel_enable_double_tap_detection(bool on);

/**
 * @brief Check whether double tap detection is enabled.
 *
 * @return True if double tap detection is enabled.
 */
bool accel_get_double_tap_detection_enabled(void);

/**
 * @brief Deliver a new sample to the service.
 *
 * Implemented by the service. Called from thread context, with samples in increasing time
 * order.
 *
 * @note May be called from within any accelerometer driver function. Avoid calling driver
 * functions from it to prevent reentrancy issues.
 *
 * @param data Sample, valid only for the duration of the call.
 */
extern void accel_cb_new_sample(AccelDriverSample const *data);

/**
 * @brief Deliver a batch of new samples to the service.
 *
 * Implemented by the service. Equivalent to calling accel_cb_new_sample() once per sample,
 * oldest first, but lets the service scale and copy the whole batch in one pass. The same
 * context and reentrancy rules apply.
 *
 * @param batch Batch description; the pointers in it are valid only for the duration of the
 *              call.
 */
extern void accel_cb_new_samples(AccelRawBatch const *batch);

/**
 * @brief Report a detected shake to the service.
 *
 * Implemented by the service. Filtering out shakes caused by the vibration motor is up to the
 * implementer.
 *
 * @param axis Axis the shake was detected on.
 * @param direction Positive or negative to tell the direction along @p axis.
 */
extern void accel_cb_shake_detected(IMUCoordinateAxis axis, int32_t direction);

/**
 * @brief Report a detected double tap to the service.
 *
 * Implemented by the service.
 *
 * @param axis Axis the double tap was detected on.
 * @param direction Positive or negative to tell the direction along @p axis.
 */
extern void accel_cb_double_tap_detected(IMUCoordinateAxis axis, int32_t direction);

/** @brief Driver work to be run from thread context. */
typedef void (*AccelOffloadCallback)(void);

/**
 * @brief Run driver work in thread context, from an ISR.
 *
 * Implemented by the service: @p cb runs on the timer task with the accelerometer service
 * lock held. Must be called from an ISR.
 *
 * @param cb Work to run.
 */
extern void accel_offload_work_from_isr(AccelOffloadCallback cb);

/**
 * @brief Run driver work in thread context.
 *
 * Implemented by the service: @p cb runs on the timer task with the accelerometer service
 * lock held.
 *
 * @param cb Work to run.
 */
extern void accel_offload_work(AccelOffloadCallback cb);

/**
 * @brief Select high or normal shake sensitivity.
 *
 * In high sensitivity mode the detection threshold is at its minimum, so that any minor motion
 * triggers a shake event; otherwise the threshold set by accel_set_shake_sensitivity_percent()
 * applies. Does not enable shake detection.
 *
 * @param sensitivity_high True for high sensitivity, false for normal.
 */
void accel_set_shake_sensitivity_high(bool sensitivity_high);

/**
 * @brief Set the normal shake sensitivity.
 *
 * Does not enable shake detection.
 *
 * @param percent Sensitivity from 0 (highest threshold, least sensitive) to 100 (lowest
 *                threshold, most sensitive).
 */
void accel_set_shake_sensitivity_percent(uint8_t percent);

/** @} */
