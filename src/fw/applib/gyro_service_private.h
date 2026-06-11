/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gyro_service.h"

typedef struct GyroServiceState {
  // Configuration for our data callback subscription to the gyro manager
  GyroManagerState *manager_state;
  GyroSamplingRate sampling_rate;
  uint16_t         samples_per_update;
  GyroRawData      *raw_data;   // of size samples_per_update

  // User-provided callbacks
  GyroDataHandler data_handler;
  GyroRawDataHandler raw_data_handler;
} GyroServiceState;

//! Initialize an existing state object
void gyro_service_state_init(GyroServiceState *state);

GyroServiceState *gyro_service_private_get_session(PebbleTask task);

void gyro_service_cleanup_task_session(PebbleTask task);
