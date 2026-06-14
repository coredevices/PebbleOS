/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "accel_service.h"
#include "event_service_client.h"

#include "system/logging.h"

typedef struct AccelServiceState {
  // Configuration for our data callback subscription to the accel manager
  AccelManagerState *manager_state;
  AccelSamplingRate sampling_rate;
  bool              deferred_free;
  uint16_t          samples_per_update;
  AccelRawData      *raw_data;   // of size samples_per_update

  // User-provided callbacks for various events
  AccelDataHandler data_handler;
  AccelTapHandler shake_handler;
  AccelTapHandler double_tap_handler;
  AccelRawDataHandler raw_data_handler;
  AccelRawDataHandler__deprecated raw_data_handler_deprecated;

  // Configuration for our other types of events
  EventServiceInfo accel_shake_info;
  EventServiceInfo accel_double_tap_info;

#if LOG_DOMAIN_ACCEL
  uint64_t prev_timestamp_ms;
#endif
} AccelServiceState;

//! Initialize an existing state object
void accel_service_state_init(AccelServiceState *state);

AccelServiceState* accel_service_private_get_session(PebbleTask task);

void accel_service_cleanup_task_session(PebbleTask task);

//! Create a new accel session. Used by kernel clients only. Kernel clients MUST use the
//! AccelSession based calls (accel_session_data_subscribe, etc.) to access the
//! accel service whereas apps use the accel_service based calls (accel_data_service_subscribe,
//! etc.). The accel_.*_service_.* calls are simply wrapper functions that look up the
//! AccelServiceState given the current task_id (app or worker) and then called into the respective
//! accel_session_.* call.
//! @return A non-zero session upon success, NULL if error
AccelServiceState* accel_session_create(void);

//! Delete an accel session created by accel_session_create. Used by kernel clients only.
//! @param session An Accel session created by accel_session_create()
void accel_session_delete(AccelServiceState *session);

//! Subscribe to the accelerometer shake event service by session ref. Used by kernel clients
//! only.
//! @param session An Accel session created by accel_session_create()
//! @param handler A callback to be executed on shake event
void accel_session_shake_subscribe(AccelServiceState *session, AccelTapHandler handler);

//! Unsubscribe from the accelerometer shake event service by session ref. Used by kernel clients
//! only.
//! @param session An Accel session created by accel_session_create()
void accel_session_shake_unsubscribe(AccelServiceState *session);

//! Subscribe to the accelerometer double tap event service by session ref. Used by kernel clients
//! only.
//! @param session An Accel session created by accel_session_create()
//! @param handler A callback to be executed on tap event
void accel_session_double_tap_subscribe(AccelServiceState *session, AccelTapHandler handler);

//! Unsubscribe from the accelerometer double tap event service by session ref. Used by kernel
//! clients only.
//! @param session An Accel session created by accel_session_create()
void accel_session_double_tap_unsubscribe(AccelServiceState *session);

//! Subscribe to the accelerometer data event service by session ref. Used by kernel clients
//! only.
//! @param session An accel session created by accel_session_create()
//! @param handler A callback to be executed on accelerometer data events
//! @param samples_per_update the number of samples to buffer, between 0 and 25.
void accel_session_data_subscribe(AccelServiceState *session, uint32_t samples_per_update,
                                  AccelDataHandler handler);

//! Subscribe to the accelerometer data event service by session ref. Used by kernel clients
//! only.
//! @param session An accel session created by accel_session_create()
//! @param sampling_rate the desired sampling_rate
//! @param samples_per_update the number of samples to buffer, between 0 and 25.
//! @param handler A callback to be executed on accelerometer data events. The callback will
//!                execute on the current task calling this function.
void accel_session_raw_data_subscribe(
    AccelServiceState *session, AccelSamplingRate sampling_rate, uint32_t samples_per_update,
    AccelRawDataHandler handler);

//! Unsubscribe from the accelerometer data event service. Used by kernel clients
//! only.
//! @param session An accel session created by accel_session_create()
void accel_session_data_unsubscribe(AccelServiceState *session);

//! Change the accelerometer sampling rate. Used by kernel clients only.
//! @param session An accel session created by accel_session_create()
//! @param rate The sampling rate in Hz (10Hz, 25Hz, 50Hz, and 100Hz possible)
int accel_session_set_sampling_rate(AccelServiceState *session, AccelSamplingRate rate);

//! Change the number of samples buffered between each accelerometer data event. Used by kernel
//! clients only.
//! @param session An accel session created by accel_session_create()
//! @param num_samples the number of samples to buffer, between 0 and 25.
int accel_session_set_samples_per_update(AccelServiceState *session, uint32_t num_samples);
