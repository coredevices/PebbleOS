/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

/*! Gyroscope driver interface
 *  ==========================
 *
 * The gyroscope driver mirrors the accelerometer driver interface (see
 * drivers/accel.h for the design rationale): it is a dumb interface to the
 * gyroscope hardware with no circular buffers, clients, threads or
 * subsampling. All of that is handled by higher level code (the gyro
 * manager service).
 *
 * Power: the driver must keep the gyroscope powered down whenever no data
 * has been requested (sampling interval and num samples both zero). Unlike
 * the accelerometer, the gyroscope is comparatively power hungry, so there
 * are no always-on event detection features (shake, tap) in this interface.
 *
 * Note that gyroscope hardware typically needs tens of milliseconds to
 * produce valid samples after power-on. The driver is responsible for
 * discarding samples taken during the turn-on settling time; callers may
 * therefore observe a short delay between requesting data and the first
 * call to gyro_cb_new_sample().
 */

typedef struct {
  //! Timestamp of when the sample was taken in microseconds since the epoch.
  //! The precision of the timestamp is not guaranteed.
  uint64_t timestamp_us;
  //! Angular velocity around the x axis in millidegrees per second.
  int32_t x;
  //! Angular velocity around the y axis in millidegrees per second.
  int32_t y;
  //! Angular velocity around the z axis in millidegrees per second.
  int32_t z;
} GyroDriverSample;

//! Initialize the gyroscope. The gyroscope must be left powered down.
void gyro_init(void);

//! Power up the gyroscope hardware
void gyro_power_up(void);

//! Power down the gyroscope hardware
void gyro_power_down(void);

//! Sets the gyroscope sampling interval.
//!
//! Not all sampling intervals are supported by all drivers. The driver must
//! select a sampling interval which is equal to or shorter than the requested
//! interval, saturating at the shortest interval supported by the hardware.
//!
//! @param interval_us The requested sampling interval in microseconds.
//!   An interval of 0 requests that the gyroscope be powered down.
//!
//! @return The actual sampling interval set by the driver.
uint32_t gyro_set_sampling_interval(uint32_t interval_us);

//! Returns the currently set gyroscope sampling interval.
uint32_t gyro_get_sampling_interval(void);

//! Set the max number of samples the driver may batch.
//!
//! Semantics are identical to accel_set_num_samples() (see drivers/accel.h):
//! n=0 means gyro_cb_new_sample() must not be called, n=1 requests immediate
//! per-sample delivery, n>1 allows the driver to batch up to n samples for
//! power savings.
void gyro_set_num_samples(uint32_t num_samples);

//! Peek at the most recent gyroscope sample.
//!
//! @param[out] data Pointer to a buffer to write the gyroscope sample to
//!
//! @return 0 if successful, nonzero on failure. Fails if the gyroscope is
//!   currently powered down.
int gyro_peek(GyroDriverSample *data);

//! Function called by the driver whenever a new gyro sample is available.
//!
//! @param[in] data pointer to a populated GyroDriverSample struct. The pointer
//!   is only valid for the duration of the function call.
//!
//! This function will always be called with samples monotonically increasing
//! in time, from a thread context. Implemented by the gyro manager service;
//! the implementation is responsible for its own locking.
extern void gyro_cb_new_sample(GyroDriverSample const *data);

//! Function called by the driver when it needs to offload work to a thread
//! context. Implemented by the gyro manager service.
typedef void (*GyroOffloadCallback)(void);
extern void gyro_offload_work(GyroOffloadCallback cb);
