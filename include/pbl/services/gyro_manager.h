/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gyro_manager_types.h"

#include "kernel/pebble_tasks.h"

#include <stdbool.h>
#include <stdint.h>

//! Gyroscope manager service.
//!
//! Kernel-side owner of the gyroscope driver (drivers/gyro.h), mirroring the
//! accelerometer manager: subscribers share a circular sample buffer with
//! per-client subsampling and receive batched delivery callbacks on their own
//! task. The gyroscope hardware is powered down whenever there are no
//! subscribers, since it draws considerably more current than the
//! accelerometer (hundreds of µA while running).

//! Maximum number of samples a subscriber may batch per update
//! (~1 second of data at the maximum native rate the manager targets).
static const unsigned int GYRO_MAX_SAMPLES_PER_UPDATE = 104;

typedef void (*GyroDataReadyCallback)(void *context);

typedef struct GyroManagerState GyroManagerState;

void gyro_manager_init(void);
void gyro_manager_enable(bool on);

// Peek interface
///////////////////////////////////////////////////////////

//! Read the current gyroscope sample. Does not power the gyroscope up: fails
//! when no subscription is keeping it running.
//! @return 0 on success, nonzero on failure
int sys_gyro_manager_peek(GyroData *gyro_data);

// Callback interface
///////////////////////////////////////////////////////////

//! Subscribe to data events. The supplied callback will be called with the supplied context
//! whenever new data is available in the buffer that was previously supplied to
//! sys_gyro_manager_set_sample_buffer. The callback will be called on the handler_task task.
//!
//! @return A GyroManagerState object that has been allocated on the kernel heap. You must call
//!         sys_gyro_manager_data_unsubscribe to free this object when you're done.
GyroManagerState* sys_gyro_manager_data_subscribe(
    GyroSamplingRate rate, GyroDataReadyCallback data_cb, void* context,
    PebbleTask handler_task);

//! @return true if an unprocessed data event is outstanding
bool sys_gyro_manager_data_unsubscribe(GyroManagerState *state);

//! Configure an existing subscription to use a given sample rate. Jitter-inducing subsampling
//! may be used to accomplish the desired rate.
int sys_gyro_manager_set_sampling_rate(GyroManagerState *state, GyroSamplingRate rate);

int sys_gyro_manager_set_sample_buffer(GyroManagerState *state, GyroRawData *buffer,
                                       uint32_t samples_per_update);

uint32_t sys_gyro_manager_get_num_samples(GyroManagerState *state, uint64_t *timestamp_ms);
bool sys_gyro_manager_consume_samples(GyroManagerState *state, uint32_t samples);
