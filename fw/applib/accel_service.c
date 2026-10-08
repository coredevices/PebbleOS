/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "accel_service.h"

#include "accel_service_private.h"
#include "event_service_client.h"

#include <pbl/services/accel_manager.h>

#include <applib/applib_malloc.auto.h>
#include <kernel/pbl_malloc.h>
#include <process_state/app_state/app_state.h>
#include <process_state/worker_state/worker_state.h>
#include <syscall/syscall.h>
#include <system/passert.h>

static bool prv_is_session_task(void) {
  PebbleTask task = pebble_task_get_current();
  return (task == PebbleTask_KernelMain || task == PebbleTask_KernelBackground ||
          task == PebbleTask_App);
}

// ------------------------------------------------------------------------------------------
// Assert that the current task is allowed to create/delete a session
static void prv_assert_session_task(void) {
  PBL_ASSERTN(prv_is_session_task());
}

// Data event context. state is NULL once the subscription is dropped
typedef struct AccelSubscriptionToken {
  AccelServiceState *state;
  //! Buffer the running handler started with, NULL outside one. Freed only once it returns
  AccelRawData *handed_raw_data;
} AccelSubscriptionToken;

// Assert that a session's data subscription is changed on the task its events run on
static void prv_assert_handler_task(AccelServiceState *state) {
  PBL_ASSERTN(state->handler_task == PebbleTask_Unknown ||
              state->handler_task == pebble_task_get_current());
}

// --------------------------------------------------------------------------------------------
// Return the session ref for the given task. This should ONLY be used by 3rd party tasks
// (app or worker).
AccelServiceState *accel_service_private_get_session(PebbleTask task) {
  if (task == PebbleTask_Unknown) {
    task = pebble_task_get_current();
  }

  if (task == PebbleTask_App) {
    return app_state_get_accel_state();
  } else if (task == PebbleTask_Worker) {
    return worker_state_get_accel_state();
  } else {
    WTF;
  }
}

void accel_service_cleanup_task_session(PebbleTask task) {
  AccelServiceState *state = accel_service_private_get_session(task);
  if (state->manager_state) {
    sys_accel_manager_data_unsubscribe(state->manager_state);
    state->manager_state = NULL;
  }
}

// ----------------------------------------------------------------------------------------------
// Event service handler for tap events
static void prv_do_shake_handle(PebbleEvent *e, void *context) {
  PebbleTask task = pebble_task_get_current();
  AccelServiceState *state = (AccelServiceState *)accel_service_private_get_session(task);
  PBL_ASSERTN(state->shake_handler != NULL);

  state->shake_handler((AccelAxisType)e->accel_tap.axis, e->accel_tap.direction);
}

// ----------------------------------------------------------------------------------------------
static void prv_do_double_tap_handle(PebbleEvent *e, void *context) {
  PebbleTask task = pebble_task_get_current();
  AccelServiceState *state = (AccelServiceState *)accel_service_private_get_session(task);
  PBL_ASSERTN(state->double_tap_handler != NULL);
  // only kernel clients can subscribe to double tap right now, so just increment double tap count
  // device analytic here
  state->double_tap_handler((AccelAxisType)e->accel_tap.axis, e->accel_tap.direction);
}

// ---------------------------------------------------------------------------------------------
// Handles one batch. Returns true if the manager already has the next one for this event
static bool prv_do_data_handle_chunk(AccelSubscriptionToken *token) {
  AccelServiceState *state = token->state;
  // The subscription was dropped after this event was posted
  if (!state) {
    applib_free(token);
    return false;
  }

  uint64_t timestamp_ms;
  uint32_t generation;
  bool more;
  uint32_t num_samples =
      sys_accel_manager_get_num_samples(state->manager_state, &timestamp_ms, &generation);
  // The batch size is 0 or the buffer changed since this event was posted, so just release it. A
  // full batch at the new size comes round on the next pass
  if (state->samples_per_update == 0 || num_samples < state->samples_per_update) {
    sys_accel_manager_consume_samples(state->manager_state, 0, generation, &more);
    return more;
  }

  PBL_ASSERTN(state->data_handler != NULL || state->raw_data_handler != NULL ||
              state->raw_data_handler_deprecated != NULL);

  uint16_t time_interval_ms = 1000 / state->sampling_rate;
  uint32_t time_since_last_sample =
      (state->prev_timestamp_ms != 0) ? timestamp_ms - state->prev_timestamp_ms : 0;
  state->prev_timestamp_ms = timestamp_ms;

  PBL_LOG_VERBOSE("got %d samples for task %d at %" PRIu32 " (%" PRIu32 " ms delta)",
                  (int)num_samples, (int)pebble_task_get_current(), (uint32_t)timestamp_ms,
                  time_since_last_sample);

  for (unsigned int i = 0; i < num_samples; i++) {
    PBL_LOG_VERBOSE("  => x:%d, y:%d, z:%d", state->raw_data[i].x, state->raw_data[i].y,
                    state->raw_data[i].z);
  }

  // Kept until the handler returns, since a raw handler reads it directly
  token->handed_raw_data = state->raw_data;
  if (state->raw_data_handler_deprecated) {
    state->raw_data_handler_deprecated(state->raw_data, num_samples);

  } else if (state->raw_data_handler) {
    state->raw_data_handler(state->raw_data, num_samples, timestamp_ms);

  } else {
    AccelData data[num_samples];
    for (uint32_t i = 0; i < num_samples; i++) {
      data[i] = (AccelData){
        .x = state->raw_data[i].x,
        .y = state->raw_data[i].y,
        .z = state->raw_data[i].z,
        .timestamp = timestamp_ms,
        .did_vibrate = sys_vibe_history_was_vibrating(timestamp_ms)
      };
      timestamp_ms += time_interval_ms;
    }
    state->data_handler(data, num_samples);
  }

  AccelRawData *handed_raw_data = token->handed_raw_data;
  token->handed_raw_data = NULL;
  // Dropped in the handler, so the state may be gone and the token owns the buffer it handed out
  if (!token->state) {
    applib_free(handed_raw_data);
    applib_free(token);
    return false;
  }

  // Free the buffer the handler started with if it swapped it out
  if (handed_raw_data && handed_raw_data != state->raw_data) {
    applib_free(handed_raw_data);
  }

  // Tell accel_manager that it can put more data in now
  bool success =
      sys_accel_manager_consume_samples(state->manager_state, num_samples, generation, &more);
  PBL_ASSERTN(success);
  return more;
}

// ---------------------------------------------------------------------------------------------
// Called by sys_accel_manager when we have data available for this subscriber
static void prv_do_data_handle(void *context) {
  // Process in chunks to limit the amount of stack space we use up
  while (prv_do_data_handle_chunk(context)) {
  }
}

// -----------------------------------------------------------------------------------------------
int accel_service_set_sampling_rate(AccelSamplingRate rate) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  return accel_session_set_sampling_rate(session, rate);
}

// ----------------------------------------------------------------------------------------------
int accel_service_set_samples_per_update(uint32_t samples_per_update) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  return accel_session_set_samples_per_update(session, samples_per_update);
}

// ----------------------------------------------------------------------------------------------
// Frees the sample buffer, unless the running handler started with it
static void prv_free_raw_data(AccelServiceState *state) {
  if (state->raw_data != state->token->handed_raw_data) {
    applib_free(state->raw_data);
  }
  state->raw_data = NULL;
}

// ----------------------------------------------------------------------------------------------
// Removes the subscription and its buffer. The data event still out for it frees the token
static void prv_drop_subscription(AccelServiceState *state) {
  bool queued = sys_accel_manager_data_unsubscribe(state->manager_state);
  prv_free_raw_data(state);
  state->token->state = NULL;
  if (!queued) {
    applib_free(state->token);
  }
  state->manager_state = NULL;
  state->token = NULL;
  state->handler_task = PebbleTask_Unknown;
}

// ----------------------------------------------------------------------------------------------
// Clamps the batch size and allocates its buffer. A batch size of 0 needs no buffer, and
// applib_malloc(0) returns NULL
static bool prv_alloc_raw_data(uint32_t *samples_per_update, AccelRawData **raw_data) {
  uint32_t max_samples_per_update = sys_accel_manager_get_max_samples_per_update();
  if (*samples_per_update > max_samples_per_update) {
    APP_LOG(LOG_LEVEL_WARNING, "%d samples per update requested, max is %d",
            (int)*samples_per_update, (int)max_samples_per_update);
    *samples_per_update = max_samples_per_update;
  }
  *raw_data = NULL;
  if (*samples_per_update == 0) {
    return true;
  }
  // This is a packed array of simple types and therefore shouldn't have compatibility padding
  *raw_data = applib_malloc(*samples_per_update * sizeof(AccelRawData));
  if (!*raw_data) {
    APP_LOG(LOG_LEVEL_ERROR, "Not enough memory to subscribe");
    return false;
  }
  return true;
}

// ----------------------------------------------------------------------------------------------
// Subscribing again replaces the current subscription. The new one is set up in full before the
// old one is removed, so a failure leaves the current one as it is, and the manager is never left
// without a subscriber in between.
static void prv_shared_subscribe(AccelServiceState *state, AccelSamplingRate sampling_rate,
                                 uint32_t samples_per_update, AccelDataHandler data_handler,
                                 AccelRawDataHandler raw_data_handler,
                                 AccelRawDataHandler__deprecated raw_data_handler_deprecated) {
  prv_assert_handler_task(state);
  // No handler to deliver to, so treat it as a batch size of 0
  if (!data_handler && !raw_data_handler && !raw_data_handler_deprecated) {
    samples_per_update = 0;
  }
  AccelRawData *raw_data;
  if (!prv_alloc_raw_data(&samples_per_update, &raw_data)) {
    return;
  }
  AccelSubscriptionToken *token = applib_malloc(sizeof(AccelSubscriptionToken));
  if (!token) {
    APP_LOG(LOG_LEVEL_ERROR, "Not enough memory to subscribe");
    applib_free(raw_data);
    return;
  }
  *token = (AccelSubscriptionToken){.state = state};

  PebbleTask handler_task = pebble_task_get_current();
  AccelManagerState *manager_state =
      sys_accel_manager_data_subscribe(sampling_rate, prv_do_data_handle, token, handler_task);
  if (sys_accel_manager_set_sample_buffer(manager_state, raw_data, samples_per_update) != 0) {
    sys_accel_manager_data_unsubscribe(manager_state);
    applib_free(raw_data);
    applib_free(token);
    return;
  }
  if (state->manager_state) {
    prv_drop_subscription(state);
  }
  state->manager_state = manager_state;
  state->token = token;
  state->handler_task = handler_task;
  state->sampling_rate = sampling_rate;
  state->raw_data = raw_data;
  state->samples_per_update = samples_per_update;
  state->data_handler = data_handler;
  state->raw_data_handler = raw_data_handler;
  state->raw_data_handler_deprecated = raw_data_handler_deprecated;
}

// ----------------------------------------------------------------------------------------------
void accel_data_service_subscribe(uint32_t samples_per_update, AccelDataHandler handler) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  accel_session_data_subscribe(session, samples_per_update, handler);
}

// ----------------------------------------------------------------------------------------------
void accel_raw_data_service_subscribe(uint32_t samples_per_update, AccelRawDataHandler handler) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, samples_per_update, handler);
}

// ----------------------------------------------------------------------------------------------
void accel_data_service_subscribe__deprecated(uint32_t samples_per_update,
                                              AccelRawDataHandler__deprecated handler) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  prv_shared_subscribe(session, ACCEL_SAMPLING_25HZ, samples_per_update, NULL, NULL, handler);
}

// ----------------------------------------------------------------------------------------------
void accel_data_service_unsubscribe(void) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  accel_session_data_unsubscribe(session);
}

// ----------------------------------------------------------------------------------------------
void accel_tap_service_subscribe(AccelTapHandler handler) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  accel_session_shake_subscribe(session, handler);
}

// ----------------------------------------------------------------------------------------------
void accel_tap_service_unsubscribe(void) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  accel_session_shake_unsubscribe(session);
}

// ----------------------------------------------------------------------------------------------
void accel_double_tap_service_subscribe(AccelTapHandler handler) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  accel_session_double_tap_subscribe(session, handler);
}

// ----------------------------------------------------------------------------------------------
void accel_double_tap_service_unsubscribe(void) {
  AccelServiceState *session = accel_service_private_get_session(PebbleTask_Unknown);
  accel_session_double_tap_unsubscribe(session);
}

// ----------------------------------------------------------------------------------------------
int accel_service_peek(AccelData *accel_data) {
  AccelServiceState *state = accel_service_private_get_session(PebbleTask_Unknown);

  int rc = sys_accel_manager_peek(accel_data);

  PBL_LOG_VERBOSE("peek data x:%d, y:%d, z:%d", accel_data->x, accel_data->y, accel_data->z);
  if (rc != 0 || state->raw_data_handler_deprecated || state->raw_data_handler) {
    // No timestamp info needed
    return rc;
  }

  accel_data->did_vibrate = (sys_vibe_get_vibe_strength() != 0);
  return rc;
}

// ----------------------------------------------------------------------------------------------
void accel_service_state_init(AccelServiceState *state) {
  *state = (AccelServiceState){
    .sampling_rate = ACCEL_DEFAULT_SAMPLING_RATE,
    .handler_task = PebbleTask_Unknown,
    .accel_shake_info =
        {
          .type = PEBBLE_ACCEL_SHAKE_EVENT,
          .handler = &prv_do_shake_handle,
        },
    .accel_double_tap_info = {
      .type = PEBBLE_ACCEL_DOUBLE_TAP_EVENT,
      .handler = &prv_do_double_tap_handle,
    }
  };
}

// ----------------------------------------------------------------------------------------------
// Event service handler for shake events
static void prv_session_do_shake_handle(PebbleEvent *e, void *context) {
  AccelServiceState *state = context;
  if (state->shake_handler != NULL) {
    state->shake_handler((AccelAxisType)e->accel_tap.axis, e->accel_tap.direction);
  }
}

// ----------------------------------------------------------------------------------------------
// Event service handler for double tap events
static void prv_session_do_double_tap_handle(PebbleEvent *e, void *context) {
  AccelServiceState *state = context;
  if (state->double_tap_handler != NULL) {
    state->double_tap_handler((AccelAxisType)e->accel_tap.axis, e->accel_tap.direction);
  }
}

// -----------------------------------------------------------------------------------------------
AccelServiceState *accel_session_create(void) {
  prv_assert_session_task();
  AccelServiceState *state = kernel_malloc_check(sizeof(AccelServiceState));

  *state = (AccelServiceState){
    .sampling_rate = ACCEL_DEFAULT_SAMPLING_RATE,
    .handler_task = PebbleTask_Unknown,
    .accel_shake_info =
        {
          .type = PEBBLE_ACCEL_SHAKE_EVENT,
          .handler = &prv_session_do_shake_handle,
          .context = state,
        },
    .accel_double_tap_info = {
      .type = PEBBLE_ACCEL_DOUBLE_TAP_EVENT,
      .handler = &prv_session_do_double_tap_handle,
      .context = state,
    },
  };
  return state;
}

// -----------------------------------------------------------------------------------------------
void accel_session_delete(AccelServiceState *session) {
  prv_assert_session_task();

  // we better have unsubscribed at this point
  PBL_ASSERTN(session->manager_state == NULL);

  // A data event still queued for the session only reads its token
  kernel_free(session);
}

// ----------------------------------------------------------------------------------------------
void accel_session_shake_subscribe(AccelServiceState *session, AccelTapHandler handler) {
  AccelServiceState *state = (AccelServiceState *)session;
  state->shake_handler = handler;
  event_service_client_subscribe(&state->accel_shake_info);
}

// ----------------------------------------------------------------------------------------------
void accel_session_shake_unsubscribe(AccelServiceState *state) {
  event_service_client_unsubscribe(&state->accel_shake_info);
  state->shake_handler = NULL;
}

// -----------------------------------------------------------------------------------------------
void accel_session_double_tap_subscribe(AccelServiceState *state, AccelTapHandler handler) {
  state->double_tap_handler = handler;
  event_service_client_subscribe(&state->accel_double_tap_info);
}

// -----------------------------------------------------------------------------------------------
void accel_session_double_tap_unsubscribe(AccelServiceState *state) {
  event_service_client_unsubscribe(&state->accel_double_tap_info);
  state->double_tap_handler = NULL;
}

// -----------------------------------------------------------------------------------------------
void accel_session_data_subscribe(AccelServiceState *state, uint32_t samples_per_update,
                                  AccelDataHandler handler) {
  prv_shared_subscribe(state, ACCEL_SAMPLING_25HZ, samples_per_update, handler, NULL, NULL);
}

// -----------------------------------------------------------------------------------------------
void accel_session_raw_data_subscribe(AccelServiceState *state, AccelSamplingRate sampling_rate,
                                      uint32_t samples_per_update, AccelRawDataHandler handler) {
  prv_shared_subscribe(state, sampling_rate, samples_per_update, NULL, handler, NULL);
}

// -----------------------------------------------------------------------------------------------
void accel_session_data_unsubscribe(AccelServiceState *state) {
  if (!state->manager_state) {
    return;
  }
  prv_assert_handler_task(state);
  prv_drop_subscription(state);
  state->data_handler = NULL;
  state->raw_data_handler = NULL;
  state->raw_data_handler_deprecated = NULL;
}

// -----------------------------------------------------------------------------------------------
int accel_session_set_sampling_rate(AccelServiceState *state, AccelSamplingRate rate) {
  if (!state->manager_state ||
      (!state->data_handler && !state->raw_data_handler && !state->raw_data_handler_deprecated)) {
    return -1;
  }
  prv_assert_handler_task(state);
  int result = sys_accel_manager_set_sampling_rate(state->manager_state, rate);
  // A rate the manager rejects leaves the driver sampling at the old rate, so the old rate stays.
  if (result == 0) {
    state->sampling_rate = rate;
  }
  return result;
}

// -----------------------------------------------------------------------------------------------
int accel_session_set_samples_per_update(AccelServiceState *state, uint32_t samples_per_update) {
  if (!state->manager_state || (samples_per_update > 0 && !state->data_handler &&
                                !state->raw_data_handler && !state->raw_data_handler_deprecated)) {
    return -1;
  }
  prv_assert_handler_task(state);
  AccelRawData *new_buf;
  if (!prv_alloc_raw_data(&samples_per_update, &new_buf)) {
    return -1;
  }

  int result =
      sys_accel_manager_set_sample_buffer(state->manager_state, new_buf, samples_per_update);
  if (result != 0) {
    // The manager still writes into the old buffer, so keep it
    applib_free(new_buf);
    return result;
  }

  prv_free_raw_data(state);
  state->raw_data = new_buf;
  state->samples_per_update = samples_per_update;
  return 0;
}
