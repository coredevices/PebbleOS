/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/accel_manager.h"

#include "applib/accel_service.h"
#include "applib/accel_service_private.h"
#include "console/prompt.h"
#include "drivers/imu.h"
#include "drivers/legacy/accel.h"
#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "os/mutex.h"
#include "os/tick.h"
#include "process_management/app_manager.h"
#include "process_management/worker_manager.h"
#include "services/common/analytics/analytics.h"
#include "services/common/event_service.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/regular_timer.h"
#include "services/common/system_task.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Define this to install an accel subscription from the KernelMain task for testing
//#define TEST_KERNEL_SUBSCRIPTION 1

// Handy define
#define PEBBLE_TASK_CURRENT PebbleTask_Unknown


// ----------------------------------------------------------------------------------------------
// We create one of these for each data service subscriber
typedef struct AccelSubscriberState {
  ListNode list_node;                       // Entry into the s_data_subscribers linked list
  AccelSessionRef session_ref;              // client's session_ref. This is used to lookup
                                            // the corresponding AccelSubscriberState
  SharedCircularBufferClient buffer_client;
  AccelSamplingRate sampling_rate;
  uint16_t samples_per_update;
  uint16_t subsample_numerator;
  uint16_t subsample_denominator;

  //! Which task we should call the data_cb_handler on
  PebbleTask task;
  CallbackEventCallback data_cb_handler;
  void*                 data_cb_context;

  uint64_t              timestamp_ms;      // timestamp of first item in the buffer
  AccelRawData          *raw_buffer;       // raw buffer allocated by subscriber in it's heap
  uint16_t              raw_buffer_size;   // size of buffer
  uint16_t              num_samples;       // number of samples in raw_buffer

} AccelSubscriberState;


// ----------------------------------------------------------------------------------------------
// Globals
static ListNode *s_data_subscribers = 0;
static uint8_t s_tap_subscribers_count = 0;
AccelSamplingRate s_accel_sampling_rate;
static uint8_t s_accel_samples_per_update;
static TimerID s_timer_id = TIMER_INVALID_ID;
static bool s_temp_peek_mode = false;
static bool s_enabled = false;


// NOTE: All of our event service callbacks (add/remove subscriber, control) are guaranteed to only
// be called from the KernelMain task by the event_service, but sys_accel_consume_data() is called
// from the subscriber task, so we need to guard our globals with this mutex
static PebbleRecursiveMutex *s_mutex;


// ------------------------------------------------------------------------------------
// Find AccelSubscriberState by AccelSessionRef
static bool prv_session_ref_list_filter(ListNode* node, void* data) {
  AccelSubscriberState *state = (AccelSubscriberState *)node;
  return state->session_ref == (AccelSessionRef)data;
}


// -------------------------------------------------------------------------------------------
// Get the state variables for the given task
static AccelSubscriberState* prv_subscriber_state(AccelSessionRef session) {
  if (session == NULL) {
    return NULL;
  }

  // Look for this session in our list of subscribers
  ListNode* node = list_find(s_data_subscribers, prv_session_ref_list_filter, (void*)session);
  return (AccelSubscriberState *)node;
}


// -----------------------------------------------------------------------------------------------
// Get accel timestamp value in ms
static uint64_t prv_get_timestamp(void) {
  time_t s;
  uint16_t ms;
  rtc_get_time_ms(&s, &ms);
  return ((uint64_t)s) * 1000 + ms;
}


// ---------------------------------------------------------------------------------------------
// Update the driver configuration based on requested params from each subscriber
static void prv_update_driver_config(void) {
  if (!s_enabled) {
    //do not update config when in low power mode
    return;
  }

  AccelSamplingRate highest_rate = ACCEL_SAMPLING_10HZ;
  uint32_t lowest_ms_per_update = ACCEL_MAX_SAMPLES_PER_UPDATE * 1000 / ACCEL_SAMPLING_10HZ;
  uint32_t ms_per_update;
  uint32_t num_fifo_subscribers = 0;

  // Cancel the peek restore config timer, if set
  s_temp_peek_mode = false;
  new_timer_stop(s_timer_id);

  AccelSubscriberState *state = (AccelSubscriberState *)s_data_subscribers;
  while (state) {
    if (state->sampling_rate > highest_rate) {
      highest_rate = state->sampling_rate;
    }
    if (state->samples_per_update > 0) {
      num_fifo_subscribers++;
      ms_per_update = state->samples_per_update * 1000 / state->sampling_rate;
      if (ms_per_update < lowest_ms_per_update) {
        lowest_ms_per_update = ms_per_update;
      }
    }
    state = (AccelSubscriberState *)state->list_node.next;
  }

  // Setup the subsampling numerator and denominators
  state = (AccelSubscriberState *)s_data_subscribers;
  while (state) {
    uint16_t new_num, new_den;
    if ((highest_rate % state->sampling_rate) == 0) {
      new_num = 1;
      new_den = highest_rate / state->sampling_rate;
    } else {
      PBL_ASSERTN(highest_rate == ACCEL_SAMPLING_25HZ
                  && state->sampling_rate == ACCEL_SAMPLING_10HZ);
      new_num = 2;
      new_den = 5;
    }
    if (state->subsample_numerator != new_num || state->subsample_denominator != new_den) {
      state->subsample_numerator = new_num;
      state->subsample_denominator = new_den;
    }

    PBL_LOG(LOG_LEVEL_DEBUG, "set subsampling for session %d to %d/%d", (int)state->session_ref,
            state->subsample_numerator, state->subsample_denominator);

    state = (AccelSubscriberState *)state->list_node.next;
  }

  // Configure the driver
  accel_set_sampling_rate(highest_rate);
  s_accel_sampling_rate = highest_rate;

  uint32_t num_samples = lowest_ms_per_update / (1000 / highest_rate);
  if (num_fifo_subscribers == 0) {
    num_samples = 0;
    // This is used if all subscribers are peek mode only, no FIFO needed
  } else if (num_samples == 0) {
    // Min FIFO setting is 1 deep
    num_samples = 1;
  } else if (num_samples > ACCEL_MAX_SAMPLES_PER_UPDATE) {
    num_samples = ACCEL_MAX_SAMPLES_PER_UPDATE;
  }
  accel_set_num_samples(num_samples);
  s_accel_samples_per_update = num_samples;

  PBL_LOG(LOG_LEVEL_DEBUG, "setting accel rate:%d, num_samples:%"PRIu32, highest_rate, num_samples);
}

// Switch accelerometer into and out of low power mode. This function is
// idempotent; calling it multiple times in a row with the same arguments must
// have the same result as calling it once.
void accel_manager_enable(bool on) {
  mutex_lock_recursive(s_mutex);
  bool prev = s_enabled;
  s_enabled = on;
  if (on && !prev) {
    imu_power_up();
    prv_update_driver_config();
  } else if (!on && prev) {
    imu_power_down();
  }
  mutex_unlock_recursive(s_mutex);
}


// ----------------------------------------------------------------------------------------------
static void prv_restore_fifo_mode_callback(void* data) {
  mutex_lock_recursive(s_mutex);
  {
    ACCEL_LOG_DEBUG("Restoring FIFO settings after peek");
    prv_update_driver_config();
  }
  mutex_unlock_recursive(s_mutex);
}


// ----------------------------------------------------------------------------------------------
// Reset the FIFO mode restoration timer for another N seconds
static void prv_set_restore_fifo_mode_timer(void) {
  s_temp_peek_mode = true;
  bool success = new_timer_start(s_timer_id, 5 * 1000, prv_restore_fifo_mode_callback, NULL, 0 /*flags*/);
  PBL_ASSERTN(success);
}




// -------------------------------------------------------------------------------------------
DEFINE_SYSCALL(int, sys_accel_manager_peek, AccelData *accel_data) {
  int result;

  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(accel_data, sizeof(*accel_data));
  }

  analytics_inc(ANALYTICS_DEVICE_METRIC_ACCEL_PEEK_COUNT, AnalyticsClient_System);
  PebbleTask task = pebble_task_get_current();
  if (task == PebbleTask_Worker || task == PebbleTask_App) {
    analytics_inc(ANALYTICS_APP_METRIC_ACCEL_PEEK_COUNT, AnalyticsClient_CurrentTask);
  }

  if (!accel_running()) {
    return (-1);
  }

  mutex_lock_recursive(s_mutex);
  {

    if (s_accel_samples_per_update == 0) {
      // If we are not in FIFO mode, can peek
      result = (accel_peek(accel_data));
      accel_data->timestamp = prv_get_timestamp();

    } else if (s_accel_samples_per_update == 1) {
      // Else, if the FIFO is 1 deep, we can ask the accel driver for the cached reading from the
      // last FIFO read
      if (s_temp_peek_mode) {
        prv_set_restore_fifo_mode_timer();    // Give us another N seconds
      }
      accel_get_latest_reading((AccelRawData *)accel_data);
      accel_data->timestamp = accel_get_latest_timestamp();
      result = 0;

    } else {
      uint64_t  old_timestamp_ms = accel_get_latest_timestamp();

      // Else, change the FIFO to 1 deep and wait for a reading
      ACCEL_LOG_DEBUG("setting FIFO to 1 deep for peek");
      accel_set_num_samples(1);
      s_accel_samples_per_update = 1;

      // Set a timer to restore settings after a while
      prv_set_restore_fifo_mode_timer();

      int max_loops = 12;
      result = -3;
      while (max_loops--) {
        accel_data->timestamp = accel_get_latest_timestamp();
        if (accel_data->timestamp != old_timestamp_ms) {
          accel_get_latest_reading((AccelRawData *)accel_data);
          result = 0;
          break;
        }
        vTaskDelay(milliseconds_to_ticks(10));
      }
    }
  }
  mutex_unlock_recursive(s_mutex);
  return result;
}

static bool prv_call_data_callback(AccelSubscriberState *state) {
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
      WTF; // Unsupported task for the accel manager
  }
}


// ---------------------------------------------------------------------------------------------
// Called by accel driver after it has put more data into the circular buffer
void accel_manager_dispatch_data(void) {
  ACCEL_LOG_DEBUG("entering accel_manager_dispatch_data");
  mutex_lock_recursive(s_mutex);
  {
    AccelSubscriberState* state;

    // Tell the accel driver it's OK to post another event if more data arrives.
    accel_reset_pending_accel_event();

    state = (AccelSubscriberState *)s_data_subscribers;
    while (state) {
      if (!state->raw_buffer || state->samples_per_update == 0) {
        state = (AccelSubscriberState *)state->list_node.next;
        continue;
      }

      // If buffer has room, read more data
      if (state->num_samples < state->samples_per_update) {

        // Read available data. We have to ask for a multiple of state->subsample_numerator
        uint32_t num_samples;
        uint32_t ask_for = state->samples_per_update - state->num_samples;
        ask_for = MAX(ask_for, state->subsample_numerator);
        ask_for = ask_for / state->subsample_numerator * state->subsample_numerator;
        PBL_ASSERTN(state->num_samples + ask_for <= state->raw_buffer_size);

        num_samples = accel_consume_data(state->raw_buffer + state->num_samples,       // buffer
                                         &state->buffer_client,
                                         ask_for,
                                         state->subsample_numerator,
                                         state->subsample_denominator);

        // Set the timestamp if we just put the first item in the buffer. If we emptied accel's
        // buffer, we can resync the timestamp. Otherwise, we stick to the computed timestamp
        // updated by sys_accel_manager_consume_samples().
        if (state->timestamp_ms == 0 || (state->num_samples == 0 && num_samples < ask_for)) {
          state->timestamp_ms = accel_get_latest_timestamp() - (num_samples * 1000 / state->sampling_rate);
          ACCEL_LOG_DEBUG("resyncing time");
        }

        state->num_samples += num_samples;
        analytics_add(ANALYTICS_DEVICE_METRIC_ACCEL_SAMPLE_COUNT, num_samples,
                      AnalyticsClient_System);
        PBL_ASSERTN(state->num_samples <= state->raw_buffer_size);
      }

      // If buffer is full, notify subscriber to process it
      if (state->num_samples >= state->samples_per_update) {
        prv_call_data_callback(state);

        ACCEL_LOG_DEBUG("full set of %d samples for session %d", state->num_samples,
                        (int)state->session_ref);
      }
      state = (AccelSubscriberState *)state->list_node.next;
    }
  }
  mutex_unlock_recursive(s_mutex);
}


// ---------------------------------------------------------------------------------------------
DEFINE_SYSCALL(uint32_t, sys_accel_manager_get_num_samples, AccelSessionRef session,
               uint64_t *timestamp_ms) {
  uint32_t result = 0;
  mutex_lock_recursive(s_mutex);
  {
    AccelSubscriberState* state = prv_subscriber_state(session);
    if (!state) {
      PBL_LOG(LOG_LEVEL_WARNING, "not subscribed");
      goto unlock;
    }
    result = state->num_samples;
    *timestamp_ms = state->timestamp_ms;
  }
unlock:
  mutex_unlock_recursive(s_mutex);
  return result;
}


// ---------------------------------------------------------------------------------------------
DEFINE_SYSCALL(bool, sys_accel_manager_consume_samples, AccelSessionRef session, uint32_t samples) {
  bool success = true;
  mutex_lock_recursive(s_mutex);
  {
    AccelSubscriberState* state = prv_subscriber_state(session);
    if (!state) {
      PBL_LOG(LOG_LEVEL_WARNING, "not subscribed");
      goto unlock;
    }
    if (samples != state->num_samples) {
      PBL_LOG(LOG_LEVEL_WARNING, "Wrong number of samples");
      success = false;
    } else {
      // Default timestamp for next chunk
      state->timestamp_ms += samples * 1000 / state->sampling_rate;
      state->num_samples = 0;
      // Fill it again from accel circular buffer
      accel_manager_dispatch_data();
    }
  }
unlock:
  mutex_unlock_recursive(s_mutex);
  return success;
}


// -------------------------------------------------------------------------------------------
DEFINE_SYSCALL(int, sys_accel_manager_set_sampling_rate, AccelSessionRef session,
               AccelSamplingRate rate) {
  if (rate != ACCEL_SAMPLING_10HZ && rate != ACCEL_SAMPLING_25HZ && rate != ACCEL_SAMPLING_50HZ
        && rate != ACCEL_SAMPLING_100HZ) {
    return -1;
  }
  mutex_lock_recursive(s_mutex);
  {
    AccelSubscriberState* state = prv_subscriber_state(session);
    if (!state) {
      PBL_LOG(LOG_LEVEL_WARNING, "not subscribed");
      goto unlock;
    }
    state->sampling_rate = rate;
    prv_update_driver_config();
  }
unlock:
  mutex_unlock_recursive(s_mutex);
  return 0;
}


// -------------------------------------------------------------------------------------------
DEFINE_SYSCALL(uint32_t, sys_accel_manager_get_buffer_size, AccelSessionRef session,
               uint32_t samples_per_update) {
  return samples_per_update + 1;
}


// -------------------------------------------------------------------------------------------
DEFINE_SYSCALL(int, sys_accel_manager_set_sample_buffer, AccelSessionRef session,
               AccelRawData *buffer, uint32_t buffer_size, uint32_t samples_per_update) {
  int result = 0;
  if (samples_per_update > ACCEL_MAX_SAMPLES_PER_UPDATE) {
    return -1;
  }

  // The buffer must be big enough to hold at least 1 more item to support 2/5 subsampling
  if (buffer_size < samples_per_update + 1) {
    PBL_LOG(LOG_LEVEL_ERROR, "Invalid buffer size");
    return -1;
  }

  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(buffer, samples_per_update * sizeof(AccelRawData));
  }

  mutex_lock_recursive(s_mutex);
  {
    AccelSubscriberState* state = prv_subscriber_state(session);
    if (!state) {
      PBL_LOG(LOG_LEVEL_WARNING, "not subscribed");
      result = -1;
      goto unlock;
    }

    state->raw_buffer = buffer;
    state->raw_buffer_size = buffer_size;
    state->samples_per_update = samples_per_update;
    state->num_samples = 0;
    prv_update_driver_config();
  }
unlock:
  mutex_unlock_recursive(s_mutex);
  return result;
}


// -------------------------------------------------------------------------------------------
// NOTE: This is guaranteed to be only called from the KernelMain task by the event_service
static void prv_tap_add_subscriber_cb(PebbleTask task) {
  mutex_lock_recursive(s_mutex);
  {
    if (++s_tap_subscribers_count == 1 && !s_data_subscribers) {
      PBL_LOG(LOG_LEVEL_DEBUG, "Starting accel service");
      accel_set_sampling_rate(ACCEL_SAMPLING_25HZ);
      if (!accel_start()) {
        PBL_LOG(LOG_LEVEL_ERROR, "Failed to start accel service");
      }
    }
  }
  mutex_unlock_recursive(s_mutex);
}


// -------------------------------------------------------------------------------------------
// NOTE: This is guaranteed to be only called from the KernelMain task by the event_service
static void prv_tap_remove_subscriber_cb(PebbleTask task) {
  mutex_lock_recursive(s_mutex);
  {
    PBL_ASSERTN(s_tap_subscribers_count > 0);
    if (--s_tap_subscribers_count == 0 && !s_data_subscribers) {
      PBL_LOG(LOG_LEVEL_DEBUG, "Stopping accel service");
      if (accel_running()) {
        accel_stop();
      }
    }
  }
  mutex_unlock_recursive(s_mutex);
}


// -------------------------------------------------------------------------------------------
DEFINE_SYSCALL(void, sys_accel_manager_data_unsubscribe, AccelSessionRef session) {
  mutex_lock_recursive(s_mutex);
  {
    AccelSubscriberState* state = prv_subscriber_state(session);
    if (state != NULL) {
      // Remove this subscriber and free up it's state variables
      accel_remove_consumer(&state->buffer_client);
      list_remove(&state->list_node, &s_data_subscribers /* &head */, NULL /* &tail */);
      kernel_free(state);

      // All data subscribes are also tap subscribers
      prv_tap_remove_subscriber_cb(pebble_task_get_current());

      if (!s_data_subscribers) {
        // If no one left using the data subscription, disable it
        sys_vibe_history_stop_collecting();
        accel_set_num_samples(0);
      } else {
        // Else, reconfigure for the common subset of requirements among remaining subscribers
        prv_update_driver_config();
      }
    }
  }
  mutex_unlock_recursive(s_mutex);
}


// -------------------------------------------------------------------------------------------
DEFINE_SYSCALL(void, sys_accel_manager_data_subscribe, AccelSessionRef session,
               AccelSamplingRate rate, CallbackEventCallback data_cb, void* context,
               PebbleTask handler_task) {
  mutex_lock_recursive(s_mutex);
  {
    // Remove previous subscription for this task, if there is one
    sys_accel_manager_data_unsubscribe(session);
    AccelSubscriberState *state = prv_subscriber_state(session);
    PBL_ASSERTN(state == NULL);

    state = kernel_malloc_check(sizeof(AccelSubscriberState));
    *state = (AccelSubscriberState) {
      .session_ref = session,
      .task = handler_task,
      .data_cb_handler = data_cb,
      .data_cb_context = context,
      .sampling_rate = rate,
      .samples_per_update = ACCEL_MAX_SAMPLES_PER_UPDATE,
    };
    // All data subscribers are also tap subscribers
    prv_tap_add_subscriber_cb(pebble_task_get_current());

    bool no_subscribers_before = (s_data_subscribers == NULL);
    s_data_subscribers = list_insert_before(s_data_subscribers, &state->list_node);
    if (no_subscribers_before) {
      sys_vibe_history_start_collecting();
    }

    // Add as a consumer to the accel buffer
    accel_add_consumer(&state->buffer_client);

    // Update the sampling rate and num samples of the driver considering the new subscriber's
    // request
    prv_update_driver_config();
  }
  mutex_unlock_recursive(s_mutex);
}


#ifdef TEST_KERNEL_SUBSCRIPTION
// -----------------------------------------------------------------------------------------------
static void prv_kernel_data_subscription_handler(AccelData *accel_data, uint32_t num_samples) {
  PBL_LOG(LOG_LEVEL_INFO, "Received %d accel samples for KernelMain.", num_samples);
}


// -----------------------------------------------------------------------------------------------
static void prv_kernel_tap_subscription_handler(AccelAxisType axis, int32_t direction) {
  PBL_LOG(LOG_LEVEL_INFO, "Received tap event for KernelMain, axis: %d, direction: %d", axis,
          direction);
}
#endif


// -------------------------------------------------------------------------------------------
void accel_manager_init(void) {
  s_mutex = mutex_create_recursive();
  s_timer_id = new_timer_create();

  event_service_init(PEBBLE_ACCEL_SHAKE_EVENT, &prv_tap_add_subscriber_cb,
                     &prv_tap_remove_subscriber_cb);

#ifdef TEST_KERNEL_SUBSCRIPTION
  accel_data_service_subscribe(5, prv_kernel_data_subscription_handler);
  accel_tap_service_subscribe(prv_kernel_tap_subscription_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
#endif

  imu_power_down();
}

#ifdef RECOVERY_FW
void command_accel_peek(void) {
  const bool temporarily_start = !accel_running();
  if (temporarily_start) {
    accel_start();
  }

  AccelData data;
  int result = sys_accel_manager_peek(&data);
  PBL_LOG(LOG_LEVEL_DEBUG, "result: %d", result);

  char buffer[20];
  prompt_send_response_fmt(buffer, sizeof(buffer), "X: %"PRId16, data.x);
  prompt_send_response_fmt(buffer, sizeof(buffer), "Y: %"PRId16, data.y);
  prompt_send_response_fmt(buffer, sizeof(buffer), "Z: %"PRId16, data.z);

  if (temporarily_start) {
    accel_stop();
  }
}
#endif

// The accelerometer should issue a shake/tap event with any slight movements when stationary.
// This will allow the watch to immediately return to normal mode, and attempt to reconnect to
// the phone.
void accel_enable_high_sensitivity(bool high_sensitivity) {
  mutex_lock_recursive(s_mutex);
  accel_set_shake_sensitivity_high(high_sensitivity);
  mutex_unlock_recursive(s_mutex);
}

void accel_manager_set_data_callback_task(AccelSessionRef session, PebbleTask task) {
  mutex_lock_recursive(s_mutex);

  AccelSubscriberState* state = prv_subscriber_state(session);
  state->task = task;

  mutex_unlock_recursive(s_mutex);
}
