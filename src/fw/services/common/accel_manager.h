/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "accel_manager_types.h"

#include "kernel/pebble_tasks.h"

#include <stdbool.h>
#include <stdint.h>


#define ACCEL_LOG_DEBUG(fmt, args...) PBL_LOG_D(LOG_DOMAIN_ACCEL, LOG_LEVEL_DEBUG, fmt, ## args)

typedef void (*AccelDataReadyCallback)(void *context);

typedef struct AccelManagerState AccelManagerState;

#if PLATFORM_ASTERIX || PLATFORM_OBELIX
static const unsigned int ACCEL_MAX_SAMPLES_PER_UPDATE = 26 * 2; // wake every 2 seconds -- lsm6dso is 26Hz
#else
static const unsigned int ACCEL_MAX_SAMPLES_PER_UPDATE = 25;
#endif


void accel_manager_init(void);
void accel_manager_enable(bool on);

// Peek interface
///////////////////////////////////////////////////////////

int sys_accel_manager_peek(AccelData *accel_data);

// Callback interface
///////////////////////////////////////////////////////////

//! Subscribe to data events. The supplied callback will be called with the supplied context
//! whenever new data is available in the buffer that was previously supplied to
//! sys_accel_manager_set_sample_buffer. The callback will be called on the handler_task task.
//!
//! @return An AccelManagerState object that has been allocated on the kernel heap. You must call
//!         sys_accel_manager_data_unsubscribe to free this object when you're done.
AccelManagerState* sys_accel_manager_data_subscribe(
    AccelSamplingRate rate, AccelDataReadyCallback data_cb, void* context,
    PebbleTask handler_task);

//! @return true if an unprocessed data event is outstanding
bool sys_accel_manager_data_unsubscribe(AccelManagerState *state);

//! Configured an existing subscription to use a given sample rate. Jitter-inducing subsampling
//! may be used to accomplish the desired rate.
int sys_accel_manager_set_sampling_rate(AccelManagerState *state, AccelSamplingRate rate);

//! Reconfigure an existing subscription to use a sampling rate that's the lowest the hardware
//! can support without introducing jitter and is at least min_rate_hz.
//!
//! @param min_rate_hz The lowest desired sample rate in millihertz.
//! @return The resulting sample rate in millihertz. 0 if it's not possible to get a rate high
//!         enough.
uint32_t accel_manager_set_jitterfree_sampling_rate(AccelManagerState *state,
                                                    uint32_t min_rate_mHz);

int sys_accel_manager_set_sample_buffer(AccelManagerState *state, AccelRawData *buffer,
                                        uint32_t samples_per_update);

uint32_t sys_accel_manager_get_num_samples(AccelManagerState *state, uint64_t *timestamp_ms);
bool sys_accel_manager_consume_samples(AccelManagerState *state, uint32_t samples);

// Functions for internal use
///////////////////////////////////////////////////////////

bool accel_manager_run_selftest(void);
bool gyro_manager_run_selftest(void);

// Set whether the accelerometer should be in a sensitive state in order to trigger an accel tap
// event from any small movements
void accel_enable_high_sensitivity(bool high_sensitivity);

// lightweight call to determine if the watch is idle
bool accel_is_idle(void);
