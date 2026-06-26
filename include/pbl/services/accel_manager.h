/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "accel_manager_types.h"

#include "kernel/pebble_tasks.h"

#include <stdbool.h>
#include <stdint.h>


typedef void (*AccelDataReadyCallback)(void *context);

typedef struct AccelManagerState AccelManagerState;

//! Returns the maximum number of samples that can be batched per update, i.e.
//! the depth of the accelerometer's hardware FIFO.
uint32_t sys_accel_manager_get_max_samples_per_update(void);


void accel_manager_init(void);
void accel_manager_enable(bool on);

// Enable or disable the kernel's shake subscription used for the motion backlight feature.
// When disabled, the accelerometer shake detection is only active if apps have subscribed.
void accel_manager_set_motion_backlight_enabled(bool enabled);

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

// Set whether the accelerometer should be in a sensitive state in order to trigger an accel tap
// event from any small movements
void accel_enable_high_sensitivity(bool high_sensitivity);

// Update the motion sensitivity based on user preference (0-100%)
// Only available on Asterix/Obelix platforms
// 100 = most sensitive, 0 = least sensitive
void accel_manager_update_sensitivity(uint8_t sensitivity_percent);

// lightweight call to determine if the watch is idle
bool accel_is_idle(void);
