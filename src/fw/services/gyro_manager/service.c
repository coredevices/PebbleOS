/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/gyro_manager.h"

#include "console/prompt.h"
#include "drivers/gyro.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "mcu/interrupts.h"
#include "os/mutex.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/system_task.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/list.h"
#include "util/math.h"
#include "util/shared_circular_buffer.h"

#include "FreeRTOS.h"
#include "queue.h"

#include <inttypes.h>
#include <string.h>

PBL_LOG_MODULE_DEFINE(service_gyro_manager, CONFIG_SERVICE_GYRO_MANAGER_LOG_LEVEL);

// This service is a structural mirror of the accelerometer manager
// (services/accel_manager); see that implementation for additional context.
// Differences: there are no event-detection features (shake/tap), and the
// driver is fully powered down whenever there are no subscribers, since the
// gyroscope draws considerably more current than the accelerometer.

#define US_PER_SECOND (1000 * 1000)

// We create one of these for each data service subscriber
typedef struct GyroManagerState {
  ListNode list_node;                       // Entry into the s_data_subscribers linked list

  //! Client pointing into s_buffer
  SubsampledSharedCircularBufferClient buffer_client;
  //! The sampling interval we've promised to this client after subsampling.
  uint32_t sampling_interval_us;
  //! The requested number of samples needed before calling data_cb_handler
  uint16_t samples_per_update;

  //! Which task we should call the data_cb_handler on
  PebbleTask task;
  GyroDataReadyCallback data_cb_handler;
  void                  *data_cb_context;

  uint64_t              timestamp_ms;      // timestamp of first item in the buffer
  GyroRawData           *raw_buffer;       // raw buffer allocated by subscriber
  uint8_t               num_samples;       // number of samples in raw_buffer
  bool                  event_posted;      // True if we've posted a "data ready" callback event
} GyroManagerState;

typedef struct {
  GyroRawData rawdata;
  // The exact time the sample was collected can be recovered by:
  //   time_sample_collected = s_last_empty_timestamp_ms + timestamp_delta_ms
  uint16_t timestamp_delta_ms;
} GyroManagerBufferData;
_Static_assert(offsetof(GyroManagerBufferData, rawdata) == 0,
    "GyroRawData must be first entry in GyroManagerBufferData struct");

// Statics
//! List of all registered consumers of gyro data. Points to GyroManagerState objects.
static ListNode *s_data_subscribers = NULL;
//! Mutex locking all gyro_manager state
static PebbleRecursiveMutex *s_gyro_manager_mutex;

//! Circular buffer that raw gyro data is written into before being subsampled for each client
static SharedCircularBuffer s_buffer;
//! Storage for s_buffer (~2s of data at 104Hz)
static uint8_t s_buffer_storage[200 * sizeof(GyroManagerBufferData)];

static uint64_t s_last_empty_timestamp_ms = 0;

//! Whether the gyro manager is enabled (runlevel-controlled). When disabled,
//! the gyroscope hardware is powered down and callbacks are ignored.
static bool s_enabled = true;

static void prv_setup_subsampling(uint32_t sampling_interval);

//! Out of all gyro subscribers, figures out:
//! @param[out] lowest_interval_us - the lowest sampling interval requested (in microseconds)
//! @param[out] max_n_samples - the max number of samples requested for batching
//! @return The longest amount of samples which can be batched assuming we are
//!   running at the lowest_sampling_interval
static uint32_t prv_get_sample_interval_info(uint32_t *lowest_interval_us,
                                             uint32_t *max_n_samples) {
  *lowest_interval_us = (US_PER_SECOND / GYRO_SAMPLING_25HZ);
  *max_n_samples = 0;
  // Tracks which subscriber wants data most frequently. Note this is different than just
  // lowest_interval_us * max_n_samples as those values can come from 2 different subscribers
  // where we want to know which one subscriber wants the highest update frequency.
  uint32_t lowest_us_per_update = UINT32_MAX;

  GyroManagerState *state = (GyroManagerState *)s_data_subscribers;
  while (state) {
    *lowest_interval_us = MIN(state->sampling_interval_us, *lowest_interval_us);
    *max_n_samples = MAX(state->samples_per_update, *max_n_samples);

    if (state->samples_per_update > 0) {
      uint32_t us_per_update = state->samples_per_update * state->sampling_interval_us;
      lowest_us_per_update = MIN(lowest_us_per_update, us_per_update);
    }
    state = (GyroManagerState *)state->list_node.next;
  }

  if (lowest_us_per_update == UINT32_MAX) {
    // No one subscribing or no one who wants updates
    return 0;
  }

  uint32_t num_samples = lowest_us_per_update / (*lowest_interval_us);
  num_samples = MIN(num_samples, GYRO_MAX_SAMPLES_PER_UPDATE);

  return num_samples;
}

static void prv_setup_subsampling(uint32_t sampling_interval) {
  // Setup the subsampling numerator and denominators
  GyroManagerState *state = (GyroManagerState *)s_data_subscribers;
  while (state) {
    uint32_t interval_gcd = gcd(sampling_interval,
                                state->sampling_interval_us);

    // Protect against divide-by-zero if gcd returns 0 (when either input is 0)
    if (interval_gcd == 0) {
      PBL_LOG_ERR("Invalid sampling interval (sampling_interval=%" PRIu32 ", state->sampling_interval_us=%" PRIu32 "), skipping session %p",
              sampling_interval, state->sampling_interval_us, state);
      state = (GyroManagerState *)state->list_node.next;
      continue;
    }

    uint32_t numerator = sampling_interval / interval_gcd;
    uint32_t denominator = state->sampling_interval_us / interval_gcd;

    PBL_LOG_DBG("set gyro subsampling for session %p to %" PRIu32 "/%" PRIu32,
            state, numerator, denominator);
    subsampled_shared_circular_buffer_client_set_ratio(
        &state->buffer_client, numerator, denominator);
    state = (GyroManagerState *)state->list_node.next;
  }
}

//! Should be called after any change to a subscriber. Handles re-configuring
//! the gyro driver to satisfy the requirements of all consumers. With no
//! subscribers, the gyroscope hardware is fully powered down.
static void prv_update_driver_config(void) {
  if (!s_enabled || s_data_subscribers == NULL) {
    gyro_set_num_samples(0);
    gyro_set_sampling_interval(0);
    return;
  }

  uint32_t lowest_interval_us;
  uint32_t max_n_samples;
  uint32_t max_batch = prv_get_sample_interval_info(&lowest_interval_us, &max_n_samples);

  // Configure the driver sampling interval and get the actual interval that the driver is going
  // to use.
  uint32_t interval_us = gyro_set_sampling_interval(lowest_interval_us);

  prv_setup_subsampling(interval_us);

  PBL_LOG_DBG("setting gyro rate:%"PRIu32", num_samples:%"PRIu32,
          US_PER_SECOND / interval_us, max_batch);

  gyro_set_num_samples(max_batch);
}

static bool prv_call_data_callback(GyroManagerState *state) {
  switch (state->task) {
    case PebbleTask_App:
    case PebbleTask_Worker:
    case PebbleTask_KernelMain: {
      PebbleEvent event = {
        .type = PEBBLE_CALLBACK_EVENT,
        .callback = {
          .callback = state->data_cb_handler,
          .data = state->data_cb_context,
        },
      };

      QueueHandle_t queue = pebble_task_get_to_queue(state->task);
      // Note: This call may fail if the queue is full but when a new sample
      // becomes available from the driver, we will retry anyway
      return xQueueSendToBack(queue, &event, 0);
    }
    case PebbleTask_KernelBackground:
      return system_task_add_callback(state->data_cb_handler, state->data_cb_context);
    case PebbleTask_NewTimers:
      return new_timer_add_work_callback(state->data_cb_handler, state->data_cb_context);
    default:
      WTF; // Unsupported task for the gyro manager
  }
}

//! This is called every time new samples arrive from the gyro driver & every
//! time data has been drained by the gyro service. Its responsibility is
//! populating subscriber storage with new samples (at the requested sample
//! frequency) and generating a callback event on the subscriber's queue when
//! the requested number of samples have been batched
static void prv_dispatch_data(bool post_event) {
  mutex_lock_recursive(s_gyro_manager_mutex);

  GyroManagerState *state = (GyroManagerState *)s_data_subscribers;
  while (state) {
    if (!state->raw_buffer) {
      state = (GyroManagerState *)state->list_node.next;
      continue;
    }

    // if subscribed but not looking for any samples then just drop the data
    if (state->samples_per_update == 0) {
      uint16_t len = shared_circular_buffer_get_read_space_remaining(
          &s_buffer, &state->buffer_client.buffer_client);
      shared_circular_buffer_consume(
          &s_buffer, &state->buffer_client.buffer_client, len);
      state = (GyroManagerState *)state->list_node.next;
      continue;
    }

    // If buffer has room, read more data
    while (state->num_samples < state->samples_per_update) {
      // Read available data.
      GyroManagerBufferData data;
      if (!shared_circular_buffer_read_subsampled(
          &s_buffer, &state->buffer_client, sizeof(data), &data, 1)) {
        // we have drained all available samples
        break;
      }

      // We provide the real time for the first sample in the subscriber's
      // buffer; later samples are spaced at the subscriber's sampling interval
      if (state->num_samples == 0) {
        state->timestamp_ms = s_last_empty_timestamp_ms + data.timestamp_delta_ms;
      }

      memcpy(state->raw_buffer + state->num_samples, &data,
             sizeof(GyroRawData));
      state->num_samples++;
    }

    // If buffer is full, notify subscriber to process it
    if (post_event && !state->event_posted &&
        state->num_samples >= state->samples_per_update) {
      // Notify the subscriber that data is available
      state->event_posted = prv_call_data_callback(state);

      if (!state->event_posted) {
        PBL_LOG_INFO("Failed to post gyro event to task: 0x%x", (int) state->task);
      }
    }
    state = (GyroManagerState *)state->list_node.next;
  }

  mutex_unlock_recursive(s_gyro_manager_mutex);
}

/*
 * Exported APIs
 */

void gyro_manager_init(void) {
  s_gyro_manager_mutex = mutex_create_recursive();

  shared_circular_buffer_init(&s_buffer, s_buffer_storage,
      sizeof(s_buffer_storage));
}

void gyro_manager_enable(bool on) {
  mutex_lock_recursive(s_gyro_manager_mutex);
  bool prev = s_enabled;
  s_enabled = on;
  if (on && !prev) {
    prv_update_driver_config();
  } else if (!on && prev) {
    gyro_set_num_samples(0);
    gyro_set_sampling_interval(0);
  }
  mutex_unlock_recursive(s_gyro_manager_mutex);
}

static void prv_copy_gyro_sample_to_gyro_data(GyroDriverSample const *gyro_sample,
                                              GyroData *gyro_data) {
  *gyro_data = (GyroData) {
    .x = gyro_sample->x,
    .y = gyro_sample->y,
    .z = gyro_sample->z,
    .timestamp /* ms */ = (gyro_sample->timestamp_us / 1000),
  };
}

DEFINE_SYSCALL(int, sys_gyro_manager_peek, GyroData *gyro_data) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(gyro_data, sizeof(*gyro_data));
  }

  mutex_lock_recursive(s_gyro_manager_mutex);

  GyroDriverSample data;
  int result = gyro_peek(&data);
  if (result == 0 /* success */) {
    prv_copy_gyro_sample_to_gyro_data(&data, gyro_data);
  }

  mutex_unlock_recursive(s_gyro_manager_mutex);

  return result;
}

DEFINE_SYSCALL(GyroManagerState*, sys_gyro_manager_data_subscribe,
               GyroSamplingRate rate, GyroDataReadyCallback data_cb, void* context,
               PebbleTask handler_task) {
  GyroManagerState *state;

  // `handler_task` decides where prv_call_data_callback() dispatches the
  // user-supplied data_cb. For KernelMain/KernelBackground/NewTimers values
  // the callback ends up invoked directly in kernel mode — handing an
  // unprivileged app arbitrary kernel-mode code execution. Force unprivileged
  // callers onto their own task so the dispatch lands in their app/worker
  // event loop instead.
  if (PRIVILEGE_WAS_ELEVATED) {
    handler_task = pebble_task_get_current();
    if (handler_task != PebbleTask_App && handler_task != PebbleTask_Worker) {
      syscall_failed();
    }
  }

  mutex_lock_recursive(s_gyro_manager_mutex);
  {
    state = kernel_malloc_check(sizeof(GyroManagerState));
    *state = (GyroManagerState) {
      .task = handler_task,
      .data_cb_handler = data_cb,
      .data_cb_context = context,
      .sampling_interval_us = (US_PER_SECOND / rate),
      .samples_per_update = GYRO_MAX_SAMPLES_PER_UPDATE,
    };

    s_data_subscribers = list_insert_before(s_data_subscribers, &state->list_node);

    // Add as a consumer to the gyro buffer
    shared_circular_buffer_add_subsampled_client(
        &s_buffer, &state->buffer_client, 1, 1);

    // Update the sampling rate and num samples of the driver considering the new
    // subscriber's request
    prv_update_driver_config();
  }
  mutex_unlock_recursive(s_gyro_manager_mutex);

  return state;
}

// Several syscalls in this file take a GyroManagerState pointer that came from
// userspace. Without validation, an app can fabricate one and use the embedded
// fields as kernel read/write primitives. Walk the authoritative kernel-side
// subscriber list to confirm the pointer really is one we handed out. Caller
// must hold s_gyro_manager_mutex.
static bool prv_state_is_valid_subscriber(const GyroManagerState *state) {
  if (state == NULL) {
    return false;
  }
  for (ListNode *node = s_data_subscribers; node != NULL; node = node->next) {
    if ((const GyroManagerState *)node == state) {
      return true;
    }
  }
  return false;
}

static void prv_assert_state_from_user(const GyroManagerState *state) {
  if (!PRIVILEGE_WAS_ELEVATED) {
    return;
  }
  mutex_lock_recursive(s_gyro_manager_mutex);
  bool valid = prv_state_is_valid_subscriber(state);
  mutex_unlock_recursive(s_gyro_manager_mutex);
  if (!valid) {
    PBL_LOG_ERR("Rejecting unknown GyroManagerState %p from unprivileged caller", state);
    syscall_failed();
  }
}

DEFINE_SYSCALL(bool, sys_gyro_manager_data_unsubscribe, GyroManagerState *state) {
  prv_assert_state_from_user(state);
  bool event_outstanding;
  mutex_lock_recursive(s_gyro_manager_mutex);
  {
    event_outstanding = state->event_posted;
    // Remove this subscriber and free up its state variables
    shared_circular_buffer_remove_subsampled_client(
        &s_buffer, &state->buffer_client);
    list_remove(&state->list_node, &s_data_subscribers /* &head */, NULL /* &tail */);
    kernel_free(state);

    // reconfig for the common subset of requirements among remaining
    // subscribers; powers the gyroscope down when none remain
    prv_update_driver_config();
  }
  mutex_unlock_recursive(s_gyro_manager_mutex);
  return event_outstanding;
}

DEFINE_SYSCALL(int, sys_gyro_manager_set_sampling_rate,
               GyroManagerState *state, GyroSamplingRate rate) {
  prv_assert_state_from_user(state);

  // Make sure the rate is one of our externally supported fixed rates
  switch (rate) {
    case GYRO_SAMPLING_25HZ:
    case GYRO_SAMPLING_50HZ:
    case GYRO_SAMPLING_100HZ:
    case GYRO_SAMPLING_200HZ:
      break;
    default:
      return -1;
  }

  mutex_lock_recursive(s_gyro_manager_mutex);

  state->sampling_interval_us = (US_PER_SECOND / rate);
  prv_update_driver_config();

  mutex_unlock_recursive(s_gyro_manager_mutex);

  return 0;
}

DEFINE_SYSCALL(int, sys_gyro_manager_set_sample_buffer,
               GyroManagerState *state, GyroRawData *buffer, uint32_t samples_per_update) {
  prv_assert_state_from_user(state);
  if (samples_per_update > GYRO_MAX_SAMPLES_PER_UPDATE) {
    return -1;
  }

  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(buffer, samples_per_update * sizeof(GyroRawData));
  }

  mutex_lock_recursive(s_gyro_manager_mutex);
  {
    state->raw_buffer = buffer;
    state->samples_per_update = samples_per_update;
    state->num_samples = 0;
    prv_update_driver_config();
  }
  mutex_unlock_recursive(s_gyro_manager_mutex);

  return 0;
}

DEFINE_SYSCALL(uint32_t, sys_gyro_manager_get_num_samples,
               GyroManagerState *state, uint64_t *timestamp_ms) {
  prv_assert_state_from_user(state);
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(timestamp_ms, sizeof(*timestamp_ms));
  }

  mutex_lock_recursive(s_gyro_manager_mutex);

  uint32_t result = state->num_samples;
  *timestamp_ms = state->timestamp_ms;

  mutex_unlock_recursive(s_gyro_manager_mutex);
  return result;
}

DEFINE_SYSCALL(bool, sys_gyro_manager_consume_samples,
               GyroManagerState *state, uint32_t samples) {
  prv_assert_state_from_user(state);
  bool success = true;
  mutex_lock_recursive(s_gyro_manager_mutex);

  if (samples > state->num_samples) {
    PBL_LOG_ERR("Consuming more gyro samples than exist %d vs %d!",
            (int)samples, (int)state->num_samples);
    success = false;
  } else if (samples != state->num_samples) {
    PBL_LOG_DBG("Dropping %d gyro samples", (int)(state->num_samples - samples));
    success = false;
  }

  state->event_posted = false;
  state->num_samples = 0;
  // Fill it again from circular buffer
  prv_dispatch_data(state->task != pebble_task_get_current() /* post_event */);

  mutex_unlock_recursive(s_gyro_manager_mutex);
  return success;
}

/*
 * Driver Callbacks - See drivers/gyro.h header for more context
 */

static bool prv_shared_buffer_empty(void) {
  bool empty = true;
  mutex_lock_recursive(s_gyro_manager_mutex);
  {
    GyroManagerState *state = (GyroManagerState *)s_data_subscribers;
    while (state) {
      int left = shared_circular_buffer_get_read_space_remaining(
          &s_buffer, &state->buffer_client.buffer_client);
      if (left != 0) {
        empty = false;
        break;
      }
      state = (GyroManagerState *)state->list_node.next;
    }
  }
  mutex_unlock_recursive(s_gyro_manager_mutex);
  return empty;
}

void gyro_cb_new_sample(GyroDriverSample const *data) {
  if (!s_enabled) {
    return;
  }

  // May be invoked from work offloaded through either the gyro manager or the
  // accel manager (shared-FIFO drains on boards where the LSM6DSO also serves
  // as the accelerometer), so take our own lock here.
  mutex_lock_recursive(s_gyro_manager_mutex);

  if (!s_buffer.clients) {
    mutex_unlock_recursive(s_gyro_manager_mutex);
    return; // no clients so don't buffer any data
  }

  GyroManagerBufferData gyro_buffer_data;
  gyro_buffer_data.rawdata.x = data->x;
  gyro_buffer_data.rawdata.y = data->y;
  gyro_buffer_data.rawdata.z = data->z;

  if (prv_shared_buffer_empty()) {
    s_last_empty_timestamp_ms = data->timestamp_us / 1000;
  }

  // Note: the delta value overflows if the s_buffer is not drained for ~65s,
  // but there should be more than enough time for it to drain in that window
  gyro_buffer_data.timestamp_delta_ms = ((data->timestamp_us / 1000) -
      s_last_empty_timestamp_ms);

  // if we have one or more clients who fell behind reading out of the buffer,
  // we will advance them until there is enough space available for the new data
  bool rv = shared_circular_buffer_write(&s_buffer, (uint8_t *)&gyro_buffer_data,
                                         sizeof(gyro_buffer_data), false /*advance_slackers*/);
  if (!rv) {
    PBL_LOG_WRN("Gyro subscriber fell behind, truncating data");
    rv = shared_circular_buffer_write(&s_buffer, (uint8_t *)&gyro_buffer_data,
                                      sizeof(gyro_buffer_data), true /*advance_slackers*/);
  }

  PBL_ASSERTN(rv);

  prv_dispatch_data(true /* post_event */);

  mutex_unlock_recursive(s_gyro_manager_mutex);
}

static void prv_handle_gyro_driver_work_cb(void *data) {
  // The gyro manager is responsible for handling locking
  mutex_lock_recursive(s_gyro_manager_mutex);
  GyroOffloadCallback cb = data;
  cb();
  mutex_unlock_recursive(s_gyro_manager_mutex);
}

void gyro_offload_work(GyroOffloadCallback cb) {
  new_timer_add_work_callback(prv_handle_gyro_driver_work_cb, cb);
}

/*
 * Console commands
 */

void command_gyro_peek(void) {
  GyroData data = {0};

  int result = sys_gyro_manager_peek(&data);
  PBL_LOG_DBG("result: %d", result);

  char buffer[24];
  prompt_send_response_fmt(buffer, sizeof(buffer), "X: %"PRId32, data.x);
  prompt_send_response_fmt(buffer, sizeof(buffer), "Y: %"PRId32, data.y);
  prompt_send_response_fmt(buffer, sizeof(buffer), "Z: %"PRId32, data.z);
}
