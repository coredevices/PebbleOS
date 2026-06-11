/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/gyro_manager.h"
#include "kernel/pebble_tasks.h"

#include <inttypes.h>
#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup GyroscopeService
//!
//! \brief Using the Pebble gyroscope
//!
//! The GyroscopeService enables watchapps to receive angular velocity samples
//! from the gyroscope, in batches, at a fixed sampling rate. Samples report
//! rotation around each axis in millidegrees per second; axes match the
//! accelerometer's orientation (X toward the right of the watch, Y toward the
//! top, Z out of the watchface).
//!
//! Battery note: unlike the accelerometer, the gyroscope draws a significant
//! current while running (hundreds of microamps -- a continuous subscription
//! can roughly halve the watch's battery life). The hardware is only powered
//! while at least one subscription is active, so subscribe for the duration
//! of a measurement session and unsubscribe promptly when done. Short
//! sessions (even an hour or two of continuous sampling) cost only a fraction
//! of a percent of the battery.
//!
//! The gyroscope needs roughly 100 ms to start producing valid samples after
//! the first subscription powers it up; delivery of the first batch is
//! delayed accordingly.
//!     @{

#define GYRO_DEFAULT_SAMPLING_RATE GYRO_SAMPLING_100HZ

//! Callback type for gyroscope data events
//! @param data Pointer to the collected gyroscope samples.
//! @param num_samples the number of samples stored in data.
typedef void (*GyroDataHandler)(GyroData *data, uint32_t num_samples);

//! Callback type for gyroscope raw data events
//! @param data Pointer to the collected gyroscope samples.
//! @param num_samples the number of samples stored in data.
//! @param timestamp the timestamp, in ms, of the first sample.
typedef void (*GyroRawDataHandler)(GyroRawData *data, uint32_t num_samples, uint64_t timestamp);

//! Subscribe to the gyroscope data event service. Once subscribed, the handler
//! gets called every time there are new gyroscope samples available. The
//! gyroscope hardware is powered up by the first subscription.
//! @note Cannot use \ref gyro_service_peek() when subscribed to gyroscope data events.
//! @param handler A callback to be executed on gyroscope data events
//! @param samples_per_update the number of samples to buffer, between 0 and 104.
void gyro_data_service_subscribe(uint32_t samples_per_update, GyroDataHandler handler);

//! Subscribe to the gyroscope raw data event service. Once subscribed, the handler
//! gets called every time there are new gyroscope samples available. Compared to
//! \ref gyro_data_service_subscribe, the raw variant avoids a per-sample copy,
//! which matters at high sampling rates.
//! @param handler A callback to be executed on gyroscope data events
//! @param samples_per_update the number of samples to buffer, between 0 and 104.
void gyro_raw_data_service_subscribe(uint32_t samples_per_update, GyroRawDataHandler handler);

//! Unsubscribe from the gyroscope data event service. Once unsubscribed,
//! the previously registered handler will no longer be called, and, when no
//! other subscriptions remain, the gyroscope hardware is powered down.
void gyro_data_service_unsubscribe(void);

//! Change the gyroscope sampling rate.
//! @param rate The sampling rate in Hz (25Hz, 50Hz, 100Hz, and 200Hz possible)
int gyro_service_set_sampling_rate(GyroSamplingRate rate);

//! Change the number of samples buffered between each gyroscope data event
//! @param num_samples the number of samples to buffer, between 0 and 104.
int gyro_service_set_samples_per_update(uint32_t num_samples);

//! Peek at the last recorded reading.
//! @param[out] data a pointer to a pre-allocated GyroData item
//! @note Can only be used while subscribed to gyroscope data events (the
//!   gyroscope is powered down otherwise).
//! @return 0 on success, nonzero if the gyroscope is not running
int gyro_service_peek(GyroData *data);

//!     @} // end addtogroup GyroscopeService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation
