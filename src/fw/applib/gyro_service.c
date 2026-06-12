/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef CONFIG_GYRO
#error "Use fw/applib/gyro_service_stub.c on boards without a gyroscope"
#endif

#include "gyro_service.h"

#include "gyro_service_private.h"
#include "applib/applib_malloc.auto.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "process_state/worker_state/worker_state.h"
#include "pbl/services/gyro_manager.h"
#include "syscall/syscall.h"
#include "system/logging.h"
#include "system/passert.h"

// This is a structural mirror of the accelerometer's applib service
// (applib/accel_service.c) without the event-detection features (shake/tap)
// and without kernel sessions: gyro subscribers are always apps or workers,
// whose state lives in their app/worker state structs.

// --------------------------------------------------------------------------------------------
// Return the state for the given task. This should ONLY be used by 3rd party tasks
// (app or worker).
GyroServiceState *gyro_service_private_get_session(PebbleTask task) {
  if (task == PebbleTask_Unknown) {
    task = pebble_task_get_current();
  }

  if (task == PebbleTask_App) {
    return app_state_get_gyro_state();
  } else if (task == PebbleTask_Worker) {
    return worker_state_get_gyro_state();
  } else {
    WTF;
  }
}

void gyro_service_cleanup_task_session(PebbleTask task) {
  GyroServiceState *state = gyro_service_private_get_session(task);
  if (state->manager_state) {
    sys_gyro_manager_data_unsubscribe(state->manager_state);
    state->manager_state = NULL;
  }
}

// -----------------------------------------------------------------------------------------------
// Handle a chunk of data received for a data subscription. Called by prv_do_data_handle.
static uint32_t prv_do_data_handle_chunk(GyroServiceState *state, uint16_t time_interval_ms) {
  uint64_t timestamp_ms;
  uint32_t num_samples = sys_gyro_manager_get_num_samples(state->manager_state, &timestamp_ms);
  if (num_samples < state->samples_per_update) {
    return 0;
  }

  if (state->raw_data_handler) {
    state->raw_data_handler(state->raw_data, num_samples, timestamp_ms);
  } else {
    // Convert to GyroData on the heap rather than the stack: at the maximum
    // batch size this conversion buffer is ~2KB, too large for a stack VLA.
    GyroData *data = applib_malloc(num_samples * sizeof(GyroData));
    if (!data) {
      PBL_LOG_WRN("Not enough memory to deliver gyro samples, dropping them");
    } else {
      for (uint32_t i = 0; i < num_samples; i++) {
        data[i] = (GyroData) {
          .x = state->raw_data[i].x,
          .y = state->raw_data[i].y,
          .z = state->raw_data[i].z,
          .timestamp = timestamp_ms,
        };
        timestamp_ms += time_interval_ms;
      }
      state->data_handler(data, num_samples);
      applib_free(data);
    }
  }

  // Tell gyro_manager that it can put more data in now
  bool success = sys_gyro_manager_consume_samples(state->manager_state, num_samples);
  PBL_ASSERTN(success);
  return num_samples;
}

// ---------------------------------------------------------------------------------------------
// Called by sys_gyro_manager when we have data available for this subscriber
static void prv_do_data_handle(void *context) {
  GyroServiceState *state = (GyroServiceState *)context;

  if (state->manager_state == NULL) {
    // event queue is handled kernel-side, so an event may fire after we've unsubscribed
    return;
  }

  PBL_ASSERTN(state->data_handler != NULL || state->raw_data_handler != NULL);

  uint16_t time_interval_ms = 1000 / state->sampling_rate;

  // Process in chunks to limit the amount of memory we use up.
  uint32_t num_processed;
  do {
    num_processed = prv_do_data_handle_chunk(state, time_interval_ms);
  } while (num_processed);
}

// -----------------------------------------------------------------------------------------------
static int prv_set_samples_per_update(GyroServiceState *state, uint32_t samples_per_update) {
  if (samples_per_update > GYRO_MAX_SAMPLES_PER_UPDATE) {
    APP_LOG(LOG_LEVEL_WARNING, "%d samples per update requested, max is %d",
            (int)samples_per_update, GYRO_MAX_SAMPLES_PER_UPDATE);
    samples_per_update = GYRO_MAX_SAMPLES_PER_UPDATE;
  }
  if (!state->manager_state
      || (samples_per_update > 0 && !state->data_handler && !state->raw_data_handler)) {
    return -1;
  }
  GyroRawData *old_buf = state->raw_data;

  // This is a packed array of simple types and therefore shouldn't have compatibility padding
  state->raw_data = applib_malloc(samples_per_update * sizeof(GyroRawData));
  if (!state->raw_data) {
    APP_LOG(LOG_LEVEL_ERROR, "Not enough memory to subscribe");
    state->raw_data = old_buf;
    return -1;
  }
  state->samples_per_update = samples_per_update;
  int result = sys_gyro_manager_set_sample_buffer(state->manager_state, state->raw_data,
                                                  samples_per_update);
  applib_free(old_buf);
  return result;
}

// ----------------------------------------------------------------------------------------------
static void prv_shared_subscribe(GyroServiceState *state, GyroSamplingRate sampling_rate,
                                 uint32_t samples_per_update) {
  state->sampling_rate = sampling_rate;
  state->manager_state = sys_gyro_manager_data_subscribe(
      sampling_rate, prv_do_data_handle, state, pebble_task_get_current());

  prv_set_samples_per_update(state, samples_per_update);
}

// ----------------------------------------------------------------------------------------------
void gyro_data_service_subscribe(uint32_t samples_per_update, GyroDataHandler handler) {
  GyroServiceState *state = gyro_service_private_get_session(PebbleTask_Unknown);

  state->data_handler = handler;
  state->raw_data_handler = NULL;

  prv_shared_subscribe(state, GYRO_DEFAULT_SAMPLING_RATE, samples_per_update);
}

// ----------------------------------------------------------------------------------------------
void gyro_raw_data_service_subscribe(uint32_t samples_per_update, GyroRawDataHandler handler) {
  GyroServiceState *state = gyro_service_private_get_session(PebbleTask_Unknown);

  state->raw_data_handler = handler;
  state->data_handler = NULL;

  prv_shared_subscribe(state, GYRO_DEFAULT_SAMPLING_RATE, samples_per_update);
}

// ----------------------------------------------------------------------------------------------
void gyro_data_service_unsubscribe(void) {
  GyroServiceState *state = gyro_service_private_get_session(PebbleTask_Unknown);
  if (!state->manager_state) {
    return;
  }
  sys_gyro_manager_data_unsubscribe(state->manager_state);

  applib_free(state->raw_data);
  state->manager_state = NULL;
  state->raw_data = NULL;
  state->data_handler = NULL;
  state->raw_data_handler = NULL;
}

// -----------------------------------------------------------------------------------------------
int gyro_service_set_sampling_rate(GyroSamplingRate rate) {
  GyroServiceState *state = gyro_service_private_get_session(PebbleTask_Unknown);
  if (!state->manager_state || (!state->data_handler && !state->raw_data_handler)) {
    return -1;
  }
  state->sampling_rate = rate;
  return sys_gyro_manager_set_sampling_rate(state->manager_state, rate);
}

// -----------------------------------------------------------------------------------------------
int gyro_service_set_samples_per_update(uint32_t samples_per_update) {
  GyroServiceState *state = gyro_service_private_get_session(PebbleTask_Unknown);
  return prv_set_samples_per_update(state, samples_per_update);
}

// ----------------------------------------------------------------------------------------------
int gyro_service_peek(GyroData *data) {
  return sys_gyro_manager_peek(data);
}

// ----------------------------------------------------------------------------------------------
void gyro_service_state_init(GyroServiceState *state) {
  *state = (GyroServiceState) {
    .sampling_rate = GYRO_DEFAULT_SAMPLING_RATE,
  };
}
