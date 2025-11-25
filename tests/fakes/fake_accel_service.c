/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_accel_service.h"

#include <stdlib.h>
#include <time.h>

#include "applib/accel_service_private.h"
#include "system/passert.h"

static AccelDataHandler s_handler;
static AccelRawDataHandler s_raw_handler;
static uint32_t s_samples_per_update;

#define ACCEL_SESSION_REF ((AccelServiceState *)1)


void accel_data_service_subscribe(uint32_t samples_per_update, AccelDataHandler handler) {
  PBL_ASSERTN(!s_raw_handler);
  s_handler = handler;
  s_samples_per_update = samples_per_update;
}

void accel_raw_data_service_subscribe(uint32_t samples_per_update, AccelRawDataHandler handler) {
  PBL_ASSERTN(!s_handler);
  s_raw_handler = handler;
  s_samples_per_update = samples_per_update;
}

void accel_data_service_unsubscribe(void) {
  s_handler = NULL;
  s_raw_handler = NULL;
}


int accel_service_set_sampling_rate(AccelSamplingRate rate) {
  return 0;
}

void fake_accel_service_invoke_callbacks(AccelData *data, uint32_t num_samples) {
  if (s_handler) {
    s_handler(data, num_samples);
  }

  if (s_raw_handler) {
    AccelRawData raw_data[num_samples];
    for (int i = 0; i < num_samples; i++) {
      raw_data[i].x = data[i].x;
      raw_data[i].y = data[i].y;
      raw_data[i].z = data[i].z;
    }

    uint64_t timestamp = data[0].timestamp;
    s_raw_handler(raw_data, num_samples, timestamp);
  }
}


AccelServiceState * accel_session_create(void) {
  return ACCEL_SESSION_REF;
}

void accel_session_delete(AccelServiceState *session) {
}

void accel_session_data_subscribe(AccelServiceState *session, uint32_t samples_per_update,
                                  AccelDataHandler handler) {
  s_handler = handler;
  s_samples_per_update = samples_per_update;
}

void accel_session_raw_data_subscribe(
    AccelServiceState *session, AccelSamplingRate sampling_rate, uint32_t samples_per_update,
    AccelRawDataHandler handler) {
  s_raw_handler = handler;
  s_samples_per_update = samples_per_update;
}


void accel_session_data_unsubscribe(AccelServiceState *session) {
  s_handler = NULL;
  s_raw_handler = NULL;
}

int accel_session_set_sampling_rate(AccelServiceState *session, AccelSamplingRate rate) {
  return 0;
}

int accel_session_set_samples_per_update(AccelServiceState *session, uint32_t samples_per_update) {
  PBL_ASSERTN(session == ACCEL_SESSION_REF);
  s_samples_per_update = samples_per_update;
  return 0;
}


