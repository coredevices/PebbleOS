/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/analytics/analytics.h>
#include <pbl/drivers/sensor.h>
#include <pbl/drivers/vibe.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>
#include <pbl/services/accel_manager.h>
#include <pbl/services/event_service.h>
#include <pbl/services/imu/units.h>
#include <pbl/services/vibe_pattern.h>
#include <pbl/util/math.h>
#include <pbl/util/shared_cbuf.h>

#include <kernel/events.h>
#include <syscall/syscall.h>

PBL_LOG_MODULE_DEFINE(service_accel_manager, CONFIG_SERVICE_ACCEL_MANAGER_LOG_LEVEL);

// We use this as an argument to indicate a lookup of the current task
#define PEBBLE_TASK_CURRENT PebbleTask_Unknown

#define US_PER_SECOND (1000 * 1000)

typedef void (*ProcessDataHandler)(CallbackEventCallback *cb, void *data);

// We create one of these for each data service subscriber
typedef struct AccelManagerState {
  ListNode list_node; // Entry into the s_data_subscribers linked list

  //! Client pointing into s_buffer
  struct pbl_shared_cbuf_subsampled_client buffer_client;
  //! The sampling interval we've promised to this client after subsampling.
  uint32_t sampling_interval_us;
  //! The requested number of samples needed before calling data_cb_handler
  uint16_t samples_per_update;

  //! Which task we should call the data_cb_handler on
  PebbleTask task;
  CallbackEventCallback data_cb_handler;
  void *data_cb_context;

  uint64_t timestamp_ms;    // timestamp of first item in the buffer
  AccelRawData *raw_buffer; // raw buffer allocated by subscriber
  uint8_t num_samples;      // number of samples in raw_buffer
  bool event_posted;        // True while a data event is queued or running
  uint32_t generation;      // bumped on every buffer change
} AccelManagerState;

typedef struct {
  AccelRawData rawdata;
  // The exact time the sample was collected can be recovered by:
  //   time_sample_collected = s_last_empty_timestamp_ms + timestamp_delta_ms
  uint16_t timestamp_delta_ms;
} AccelManagerBufferData;
static_assert(offsetof(AccelManagerBufferData, rawdata) == 0,
              "AccelRawData must be first entry in AccelManagerBufferData struct");

// Statics
//! List of all registered consumers of accel data. Points to AccelManagerState objects.
static ListNode *s_data_subscribers = nullptr;

//! Reference count of how many shake subscribers we have. Used to turn off the feature when not
//! in use.
static uint8_t s_shake_subscribers_count = 0;
//! Reference count of how many double tap subscribers we have. Used to turn off the feature when
//! not in use.
static uint8_t s_double_tap_subscribers_count = 0;

//! Circular buffer that raw accel data is written into before being subsampled for each client
static struct pbl_shared_cbuf s_buffer;
//! Storage for s_buffer, sized to hold ~2 batches plus headroom, with a floor
//! that keeps buffering room for high-rate app subscribers.
static uint8_t s_buffer_storage[PBL_MAX(200, 2 * CONFIG_SERVICE_ACCEL_MANAGER_BATCH_SAMPLES + 50) *
                                sizeof(AccelManagerBufferData)];

static uint64_t s_last_empty_timestamp_ms = 0;

//! Whether the accel manager is enabled. When disabled (e.g. during factory reset via
//! RunLevel_BareMinimum), the accelerometer hardware is powered down and callbacks are ignored.
static bool s_enabled = true;

//! Whether the kernel is subscribed to shake events for the motion backlight feature.
static bool s_motion_backlight_subscribed = false;

// Accel Idle
#define ACCEL_MAX_IDLE_DELTA 100
static AccelData s_last_analytics_position;
static AccelData s_last_accel_data;

//! Shake sensitivity set by the user preference, applied unless high sensitivity is on
static uint8_t s_shake_sensitivity_percent = 100;
static bool s_shake_sensitivity_high;

static void prv_setup_subsampling(uint32_t sampling_interval);

static uint32_t prv_get_attr(enum pbl_sensor_attr attr) {
  int32_t val = 0;

  if (pbl_sensor_attr_get(ACCEL, PBL_SENSOR_ACCEL, attr, &val) != 0) {
    return 0;
  }

  return (uint32_t)val;
}

static uint32_t prv_get_interval(void) {
  return prv_get_attr(PBL_SENSOR_ATTR_INTERVAL);
}

static uint32_t prv_get_max_batch(void) {
  return prv_get_attr(PBL_SENSOR_ATTR_BATCH_MAX);
}

static int prv_set_interval(uint32_t interval_us, uint32_t *actual_us) {
  int32_t val = (int32_t)interval_us;
  int ret;

  ret = pbl_sensor_attr_set(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_INTERVAL, &val);
  if (ret == 0 && actual_us != nullptr) {
    *actual_us = (uint32_t)val;
  }

  return ret;
}

static void prv_set_batch(uint32_t num_samples) {
  int32_t val = (int32_t)num_samples;

  (void)pbl_sensor_attr_set(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_BATCH, &val);
}

static void prv_set_event(enum pbl_sensor_event_type type, bool on) {
  (void)pbl_sensor_set_event(ACCEL, type, on);
}

static void prv_apply_shake_sensitivity(void) {
  int32_t val = s_shake_sensitivity_high ? 100 : s_shake_sensitivity_percent;

  (void)pbl_sensor_attr_set(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_SHAKE_SENSITIVITY, &val);
}

//! Offset from uptime to the wall clock, in milliseconds
static int64_t prv_realtime_offset_ms(void) {
  return (int64_t)pbl_realtime_ms() - (int64_t)pbl_uptime_ms();
}

static void prv_shake_add_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);
  {
    if (++s_shake_subscribers_count == 1) {
      PBL_LOG_DBG("Starting accel shake service");
      prv_set_event(PBL_SENSOR_EVENT_SHAKE, true);
      prv_setup_subsampling(prv_get_interval());
    }
  }
  pbl_sensor_unlock(ACCEL);
}

static void prv_shake_remove_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);
  {
    PBL_ASSERTN(s_shake_subscribers_count > 0);
    if (--s_shake_subscribers_count == 0) {
      PBL_LOG_DBG("Stopping accel shake service");
      prv_set_event(PBL_SENSOR_EVENT_SHAKE, false);
      prv_setup_subsampling(prv_get_interval());
    }
  }
  pbl_sensor_unlock(ACCEL);
}

static void prv_double_tap_add_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);

  if (++s_double_tap_subscribers_count == 1) {
    PBL_LOG_DBG("Starting accel double tap service");
    prv_set_event(PBL_SENSOR_EVENT_DOUBLE_TAP, true);
    prv_setup_subsampling(prv_get_interval());
  }

  pbl_sensor_unlock(ACCEL);
}

static void prv_double_tap_remove_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);

  PBL_ASSERTN(s_double_tap_subscribers_count > 0);
  if (--s_double_tap_subscribers_count == 0) {
    PBL_LOG_DBG("Stopping accel double tap service");
    prv_set_event(PBL_SENSOR_EVENT_DOUBLE_TAP, false);
    prv_setup_subsampling(prv_get_interval());
  }

  pbl_sensor_unlock(ACCEL);
}

//! Out of all accel subscribers, figures out:
//! @param[out] lowest_interval_us - the lowest sampling interval requested (in microseconds)
//! @param[out] max_n_samples - the max number of samples requested for batching
//! @return The longest amount of samples which can be batched assuming we are
//!   running at the lowest_sampling_interval
//!
//! @note currently the longest interval we can batch samples for is computed
//! as the minimum of (samples to batch / sample rate) out of all the active
//! subscribers. This means that if we have two subscribers, subscriber A at
//! 200ms, and subscriber B at 250ms, new samples will become available every
//! 200ms, so subscriber B's data buffer would not fill until 400ms, resulting
//! in a 150ms latency. This is how the legacy implementation worked as well
//! but is potentially something we could improve in the future if it becomes
//! a problem.
static uint32_t prv_get_sample_interval_info(uint32_t *lowest_interval_us,
                                             uint32_t *max_n_samples) {
  *lowest_interval_us = (US_PER_SECOND / ACCEL_SAMPLING_10HZ);
  *max_n_samples = 0;
  // Tracks which subscriber wants data most frequently. Note this is different than just
  // lowest_interval_us * max_n_samples as those values can come from 2 different subscribers
  // where we want to know which one subscriber wants the highest update frequency.
  uint32_t lowest_us_per_update = UINT32_MAX;

  AccelManagerState *state = (AccelManagerState *)s_data_subscribers;
  while (state) {
    *lowest_interval_us = PBL_MIN(state->sampling_interval_us, *lowest_interval_us);
    *max_n_samples = PBL_MAX(state->samples_per_update, *max_n_samples);

    if (state->samples_per_update > 0) {
      uint32_t us_per_update = state->samples_per_update * state->sampling_interval_us;
      lowest_us_per_update = PBL_MIN(lowest_us_per_update, us_per_update);
    }
    state = (AccelManagerState *)state->list_node.next;
  }

  if (lowest_us_per_update == UINT32_MAX) {
    // No one subscribing or no one who wants updates
    return 0;
  }

  uint32_t num_samples = lowest_us_per_update / (*lowest_interval_us);
  num_samples = PBL_MIN(num_samples, prv_get_max_batch());

  return num_samples;
}

static void prv_setup_subsampling(uint32_t sampling_interval) {
  // Setup the subsampling numerator and denominators
  AccelManagerState *state = (AccelManagerState *)s_data_subscribers;
  while (state) {
    uint32_t interval_gcd = gcd(sampling_interval, state->sampling_interval_us);

    // Protect against divide-by-zero if gcd returns 0 (when either input is 0)
    // This can happen if the accelerometer driver is not initialized properly
    if (interval_gcd == 0) {
      PBL_LOG_ERR("Invalid sampling interval (sampling_interval=%" PRIu32
                  ", state->sampling_interval_us=%" PRIu32 "), skipping session %p",
                  sampling_interval, state->sampling_interval_us, state);
      state = (AccelManagerState *)state->list_node.next;
      continue;
    }

    uint32_t numerator = sampling_interval / interval_gcd;
    uint32_t denominator = state->sampling_interval_us / interval_gcd;

    PBL_LOG_DBG("set subsampling for session %p to %" PRIu32 "/%" PRIu32, state, numerator,
                denominator);
    pbl_shared_cbuf_subsampled_client_set_ratio(&state->buffer_client, numerator, denominator);
    state = (AccelManagerState *)state->list_node.next;
  }
}

//! Should be called after any change to a subscriber. Handles re-configuring
//! the accel driver to satisfy the requirements of all consumers (i.e setting
//! sampling rate and max number of samples which can be batched). If there are no
//! subscribers, chooses the lowest power configuration settings
static void prv_update_driver_config(void) {
  // TODO: Add low power support
  uint32_t lowest_interval_us;
  uint32_t max_n_samples;
  uint32_t max_batch = prv_get_sample_interval_info(&lowest_interval_us, &max_n_samples);

  // Configure the driver sampling interval and get the actual interval that the driver is going
  // to use.
  uint32_t interval_us;
  if (prv_set_interval(lowest_interval_us, &interval_us) != 0) {
    interval_us = lowest_interval_us;
  }

  prv_setup_subsampling(interval_us);

  PBL_LOG_DBG("setting accel rate:%" PRIu32 ", num_samples:%" PRIu32, US_PER_SECOND / interval_us,
              max_batch);

  prv_set_batch(max_batch);
}

static bool prv_call_data_callback(AccelManagerState *state) {
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

      struct pbl_msgq *queue = pebble_task_get_to_queue(state->task);
      // Note: This call may fail if the queue is full but when a new sample
      // becomes available from the driver, we will retry anyway
      return pbl_msgq_put(queue, &event, PBL_NO_WAIT) == 0;
    }
    case PebbleTask_SysWorkq:
      return pbl_sys_work_call(state->data_cb_handler, state->data_cb_context);
    case PebbleTask_SysWorkqHigh:
      return pbl_sys_work_call_high(state->data_cb_handler, state->data_cb_context);
    default:
      pbl_panic(); // Unsupported task for the accel manager
  }
}

//! This is called every time new samples arrive from the accel driver & every
//! time data has been drained by the accel service. Its responsibility is
//! populating subscriber storage with new samples (at the requested sample
//! frequency) and generating a callback event on the subscriber's queue when
//! the requested number of samples have been batched
static void prv_dispatch_data(bool post_event) {
  pbl_sensor_lock(ACCEL);

  AccelManagerState *state = (AccelManagerState *)s_data_subscribers;
  while (state) {
    // drop the data if none are wanted or there's no buffer, so the subscriber doesn't fall behind
    if (state->samples_per_update == 0 || !state->raw_buffer) {
      uint16_t len =
          pbl_shared_cbuf_get_read_space_remaining(&s_buffer, &state->buffer_client.buffer_client);
      pbl_shared_cbuf_consume(&s_buffer, &state->buffer_client.buffer_client, len);
      state = (AccelManagerState *)state->list_node.next;
      continue;
    }

    // If buffer has room, read more data
    while (state->num_samples < state->samples_per_update) {
      // Read available data.
      AccelManagerBufferData data;
      if (!pbl_shared_cbuf_read_subsampled(&s_buffer, &state->buffer_client, sizeof(data), &data,
                                           1)) {
        // we have drained all available samples
        break;
      }

      // Note: the accel_service currently only buffers AccelRawData (i.e it
      // does not track the timestamp explicitly.) The accel service drains a
      // buffers worth of data at a time and asks for the starting time
      // (state->timestamp_ms) of the first sample in that buffer when it
      // does. Therefore, we provide the real time for the first sample. In
      // the future, we could phase out legacy accel code and provide the
      // exact timestamp with every sample
      if (state->num_samples == 0) {
        state->timestamp_ms = s_last_empty_timestamp_ms + data.timestamp_delta_ms;
      }

      memcpy(state->raw_buffer + state->num_samples, &data, sizeof(AccelRawData));
      state->num_samples++;
    }

    // If buffer is full, notify subscriber to process it
    if (post_event && !state->event_posted && state->num_samples >= state->samples_per_update) {
      // Notify the subscriber that data is available
      state->event_posted = prv_call_data_callback(state);

      PBL_LOG_VERBOSE("full set of %d samples for session %p", state->num_samples, state);

      if (!state->event_posted) {
        PBL_LOG_ERR("Failed to post accel event to task: 0x%x", (int)state->task);
      }
    }
    state = (AccelManagerState *)state->list_node.next;
  }

  pbl_sensor_unlock(ACCEL);
}

// Compute and return the device's delta position to help determine movement as idle.
static uint32_t prv_compute_delta_pos(AccelData *cur_pos, AccelData *last_pos) {
  return (abs(last_pos->x - cur_pos->x) + abs(last_pos->y - cur_pos->y) +
          abs(last_pos->z - cur_pos->z));
}

/*
 * Exported APIs
 */

void accel_manager_set_motion_backlight_enabled(bool enabled) {
  pbl_sensor_lock(ACCEL);
  if (enabled && !s_motion_backlight_subscribed) {
    prv_shake_add_subscriber_cb(PebbleTask_KernelMain);
    s_motion_backlight_subscribed = true;
  } else if (!enabled && s_motion_backlight_subscribed) {
    prv_shake_remove_subscriber_cb(PebbleTask_KernelMain);
    s_motion_backlight_subscribed = false;
  }
  pbl_sensor_unlock(ACCEL);
}

// Update the motion sensitivity based on user preference (0-100%)
// This is called by the preferences system when the user changes the setting
void accel_manager_update_sensitivity(uint8_t sensitivity_percent) {
  // Sensitivity mapping:
  // - sensitivity_percent: 0-100 where higher = more sensitive
  // - For those that support it, this maps to the wake-up threshold
  // - Lower threshold = more sensitive (triggers on smaller movements)
  // - Higher threshold = less sensitive (requires larger movements to trigger)
  //
  // We'll map the user's percentage to a threshold multiplier:
  // - 100% (most sensitive) = use Low threshold
  // - 50% (medium) = use mid-range
  // - 0% (least sensitive) = use High threshold

  pbl_sensor_lock(ACCEL);
  s_shake_sensitivity_percent = PBL_MIN(sensitivity_percent, 100);
  prv_apply_shake_sensitivity();
  pbl_sensor_unlock(ACCEL);
}

static const struct pbl_sensor_listener s_listener;

void accel_manager_init(void) {
  pbl_shared_cbuf_init(&s_buffer, s_buffer_storage, sizeof(s_buffer_storage));
  pbl_sensor_set_listener(ACCEL, &s_listener);

  event_service_init(PEBBLE_ACCEL_SHAKE_EVENT, &prv_shake_add_subscriber_cb,
                     &prv_shake_remove_subscriber_cb);

  event_service_init(PEBBLE_ACCEL_DOUBLE_TAP_EVENT, &prv_double_tap_add_subscriber_cb,
                     &prv_double_tap_remove_subscriber_cb);

// Apply saved motion sensitivity preference for Asterix/Obelix
// Only available in normal shell (not PRF)
#if defined(CONFIG_ACCEL_SENSITIVITY) && !defined(CONFIG_RECOVERY_FW)
  extern uint8_t shell_prefs_get_motion_sensitivity(void);
  uint8_t saved_sensitivity = shell_prefs_get_motion_sensitivity();
  accel_manager_update_sensitivity(saved_sensitivity);
  PBL_LOG_DBG("Initialized motion sensitivity to %u percent", saved_sensitivity);
#endif
}

static void prv_make_accel_data(const int32_t ug[3], uint64_t timestamp_ms, AccelData *accel_data) {
  *accel_data = (AccelData){
    .x = (int16_t)(ug[0] / 1000),
    .y = (int16_t)(ug[1] / 1000),
    .z = (int16_t)(ug[2] / 1000),
    .timestamp = timestamp_ms,
    .did_vibrate = (sys_vibe_get_vibe_strength() != VIBE_STRENGTH_OFF)
  };
}

PBL_SYSCALL(int, sys_accel_manager_peek, (OUT, AccelData *, accel_data)) {
  PBL_ANALYTICS_ADD(accel_peek_count, 1);

  pbl_sensor_lock(ACCEL);

  int32_t ug[3];
  uint64_t timestamp_us;
  int result = pbl_sensor_read(ACCEL, PBL_SENSOR_ACCEL, ug, &timestamp_us);
  if (result == 0 /* success */) {
    prv_make_accel_data(ug, prv_realtime_offset_ms() + timestamp_us / 1000, accel_data);
    s_last_accel_data = *accel_data;
  }

  pbl_sensor_unlock(ACCEL);

  return result;
}

static bool prv_is_supported_rate(AccelSamplingRate rate) {
  switch (rate) {
    case ACCEL_SAMPLING_10HZ:
    case ACCEL_SAMPLING_25HZ:
    case ACCEL_SAMPLING_50HZ:
    case ACCEL_SAMPLING_100HZ:
      return true;
    default:
      return false;
  }
}

PBL_SYSCALL(AccelManagerState *, sys_accel_manager_data_subscribe, (VAL, AccelSamplingRate, rate),
            (VAL, AccelDataReadyCallback, data_cb), (VAL, void *, context),
            (VAL, PebbleTask, handler_task)) {
  AccelManagerState *state;

  // `handler_task` decides where prv_call_data_callback() dispatches the
  // user-supplied data_cb. For KernelMain/SysWorkq/SysWorkqHigh values
  // the callback ends up invoked directly in kernel mode (either via the
  // kernel main event loop's PEBBLE_CALLBACK_EVENT handler, pbl_sys_work_call,
  // or pbl_sys_work_call_high) — handing an unprivileged app arbitrary
  // kernel-mode code execution. Force unprivileged callers onto their own
  // task so the dispatch lands in their app/worker event loop instead.
  if (from_user) {
    handler_task = pebble_task_get_current();

    if (!prv_is_supported_rate(rate)) {
      rate = ACCEL_SAMPLING_25HZ;
    }
  }

  pbl_sensor_lock(ACCEL);
  {
    state = pbl_malloc_check(sizeof(AccelManagerState));
    *state = (AccelManagerState){
      .task = handler_task,
      .data_cb_handler = data_cb,
      .data_cb_context = context,
      .sampling_interval_us = (US_PER_SECOND / rate),
      .samples_per_update = prv_get_max_batch(),
    };

    bool no_subscribers_before = (s_data_subscribers == nullptr);
    s_data_subscribers = list_insert_before(s_data_subscribers, &state->list_node);
    if (no_subscribers_before) {
      sys_vibe_history_start_collecting();
    }

    // Add as a consumer to the accel buffer
    pbl_shared_cbuf_add_subsampled_client(&s_buffer, &state->buffer_client, 1, 1);

    // Update the sampling rate and num samples of the driver considering the new
    // subscriber's request
    prv_update_driver_config();
  }
  pbl_sensor_unlock(ACCEL);

  return state;
}

// Several syscalls in this file take an AccelManagerState pointer that came from
// userspace. Without validation, an app can fabricate one and use the embedded
// fields (timestamp_ms, raw_buffer, list_node, ...) as kernel read/write primitives.
// Walk the authoritative kernel-side subscriber list to confirm the pointer really
// is one we handed out. The list is short (one entry per active subscriber) so the
// walk is cheap. Caller must hold the accel device lock.
static bool prv_state_is_valid_subscriber(const AccelManagerState *state) {
  if (state == nullptr) {
    return false;
  }
  for (ListNode *node = s_data_subscribers; node != nullptr; node = node->next) {
    if ((const AccelManagerState *)node == state) {
      return true;
    }
  }
  return false;
}

static void prv_assert_state_from_user(const AccelManagerState *state, bool from_user) {
  if (!from_user) {
    return;
  }
  pbl_sensor_lock(ACCEL);
  bool valid = prv_state_is_valid_subscriber(state) && (state->task == pebble_task_get_current());
  pbl_sensor_unlock(ACCEL);
  if (!valid) {
    PBL_LOG_ERR("Rejecting unknown AccelManagerState %p from unprivileged caller", state);
    pbl_syscall_fail();
  }
}

static void prv_remove_subscriber_locked(AccelManagerState *state) {
  // Remove this subscriber and free up its state variables
  pbl_shared_cbuf_remove_subsampled_client(&s_buffer, &state->buffer_client);
  list_remove(&state->list_node, &s_data_subscribers /* &head */, nullptr /* &tail */);
  pbl_free(state);

  if (!s_data_subscribers) {
    // If no one left using the data subscription, disable it
    sys_vibe_history_stop_collecting();
  }

  // reconfig for the common subset of requirements among remaining subscribers
  prv_update_driver_config();
}

PBL_SYSCALL(bool, sys_accel_manager_data_unsubscribe, (VAL, AccelManagerState *, state)) {
  prv_assert_state_from_user(state, from_user);
  bool event_outstanding;
  pbl_sensor_lock(ACCEL);
  {
    event_outstanding = state->event_posted;
    prv_remove_subscriber_locked(state);
  }
  pbl_sensor_unlock(ACCEL);
  return event_outstanding;
}

void accel_manager_remove_task_subscriptions(PebbleTask task) {
  pbl_sensor_lock(ACCEL);
  {
    ListNode *node = s_data_subscribers;
    while (node != nullptr) {
      ListNode *next = node->next;
      AccelManagerState *state = (AccelManagerState *)node;
      if (state->task == task) {
        prv_remove_subscriber_locked(state);
      }
      node = next;
    }
  }
  pbl_sensor_unlock(ACCEL);
}

PBL_SYSCALL(int, sys_accel_manager_set_sampling_rate, (VAL, AccelManagerState *, state),
            (VAL, AccelSamplingRate, rate)) {
  prv_assert_state_from_user(state, from_user);

  // Make sure the rate is one of our externally supported fixed rates
  if (!prv_is_supported_rate(rate)) {
    return -1;
  }

  pbl_sensor_lock(ACCEL);

  state->sampling_interval_us = (US_PER_SECOND / rate);
  prv_update_driver_config();

  pbl_sensor_unlock(ACCEL);

  // TODO: doesn't look like our API specifies what this routine should return.
  return 0;
}

uint32_t accel_manager_set_jitterfree_sampling_rate(AccelManagerState *state,
                                                    uint32_t min_rate_mHz) {
  // HACK
  // We're dumb and don't support anything other than 12.5hz for jitter-free sampling. We chose
  // this rate because it divides evenly into all the native rates we support right now.
  // Supporting a wider range of jitter-free rates is harder due to dealing with all the potential
  // combinations of different subscribers asking for different rates.
  const uint32_t ONLY_SUPPORTED_JITTERFREE_RATE_MILLIHZ = 12500;
  PBL_ASSERTN(min_rate_mHz <= ONLY_SUPPORTED_JITTERFREE_RATE_MILLIHZ);

  pbl_sensor_lock(ACCEL);

  state->sampling_interval_us = (US_PER_SECOND * 1000) / ONLY_SUPPORTED_JITTERFREE_RATE_MILLIHZ;
  prv_update_driver_config();

  pbl_sensor_unlock(ACCEL);

  return ONLY_SUPPORTED_JITTERFREE_RATE_MILLIHZ;
}

PBL_SYSCALL(int, sys_accel_manager_set_sample_buffer, (VAL, AccelManagerState *, state),
            (VAL, AccelRawData *, buffer), (VAL, uint32_t, samples_per_update)) {
  prv_assert_state_from_user(state, from_user);
  if (samples_per_update > prv_get_max_batch()) {
    return -1;
  }

  // A batch size of 0 never writes the buffer, so it's neither checked nor kept
  if (from_user && samples_per_update > 0) {
    pbl_syscall_verify_user(buffer, samples_per_update * sizeof(AccelRawData));
  }

  pbl_sensor_lock(ACCEL);
  {
    state->raw_buffer = (samples_per_update > 0) ? buffer : nullptr;
    state->samples_per_update = samples_per_update;
    state->num_samples = 0;
    state->generation++;
    prv_update_driver_config();
  }
  pbl_sensor_unlock(ACCEL);

  return 0;
}

PBL_SYSCALL(uint32_t, sys_accel_manager_get_max_samples_per_update) {
  return prv_get_max_batch();
}

PBL_SYSCALL(uint32_t, sys_accel_manager_get_num_samples, (VAL, AccelManagerState *, state),
            (OUT, uint64_t *, timestamp_ms), (OUT, uint32_t *, generation)) {
  prv_assert_state_from_user(state, from_user);

  pbl_sensor_lock(ACCEL);

  uint32_t result = state->num_samples;
  *timestamp_ms = state->timestamp_ms;
  *generation = state->generation;

  pbl_sensor_unlock(ACCEL);
  return result;
}

PBL_SYSCALL(bool, sys_accel_manager_consume_samples, (VAL, AccelManagerState *, state),
            (VAL, uint32_t, samples), (VAL, uint32_t, generation), (OUT, bool *, more)) {
  prv_assert_state_from_user(state, from_user);
  bool success = true;
  pbl_sensor_lock(ACCEL);

  // Samples from a buffer that's been replaced, or none at all, leave the current buffer as it is
  if (generation == state->generation && samples > 0) {
    if (samples > state->num_samples) {
      PBL_LOG_ERR("Consuming more samples than exist %d vs %d!", (int)samples,
                  (int)state->num_samples);
      success = false;
    } else if (samples != state->num_samples) {
      PBL_LOG_DBG("Dropping %d accel samples", (int)(state->num_samples - samples));
      success = false;
    }
    state->num_samples = 0;
  }

  // Fill it again from circular buffer
  prv_dispatch_data(false /* post_event */);
  // A batch that's already in goes to the running callback, so no event is posted for it
  *more = state->samples_per_update > 0 && state->num_samples >= state->samples_per_update;
  state->event_posted = *more;

  pbl_sensor_unlock(ACCEL);
  return success;
}

/*
 * TODO: APIs that still need to be implemented
 */

void accel_manager_enable(bool on) {
  pbl_sensor_lock(ACCEL);
  bool prev = s_enabled;
  s_enabled = on;
  if (on && !prev) {
    prv_update_driver_config();
    if (s_shake_subscribers_count > 0) {
      prv_set_event(PBL_SENSOR_EVENT_SHAKE, true);
    }
    if (s_double_tap_subscribers_count > 0) {
      prv_set_event(PBL_SENSOR_EVENT_DOUBLE_TAP, true);
    }
  } else if (!on && prev) {
    prv_set_batch(0);
    (void)prv_set_interval(0, nullptr);
    prv_set_event(PBL_SENSOR_EVENT_SHAKE, false);
    prv_set_event(PBL_SENSOR_EVENT_DOUBLE_TAP, false);
  }
  pbl_sensor_unlock(ACCEL);
}

void accel_manager_exit_low_power_mode(void) {
}

// Return true if we are "idle", defined as seeing no movement in the last hour.
bool accel_is_idle(void) {
  // It was idle recently, see if it's still idle. Note we avoid reading the accel hardware
  // again here to keep this call as lightweight as possible. Instead we are just comparing the last
  // read value with the value last captured by analytics (which does so on an hourly heartbeat).
  return (prv_compute_delta_pos(&s_last_accel_data, &s_last_analytics_position) <
          ACCEL_MAX_IDLE_DELTA);
}

// The accelerometer should issue a shake/tap event with any slight movements when stationary.
// This will allow the watch to immediately return to normal mode, and attempt to reconnect to
// the phone.
void accel_enable_high_sensitivity(bool high_sensitivity) {
  pbl_sensor_lock(ACCEL);
  s_shake_sensitivity_high = high_sensitivity;
  prv_apply_shake_sensitivity();
  pbl_sensor_unlock(ACCEL);
}

/*
 * Accelerometer listener
 */

static bool prv_shared_buffer_empty(void) {
  bool empty = true;
  pbl_sensor_lock(ACCEL);
  {
    AccelManagerState *state = (AccelManagerState *)s_data_subscribers;
    while (state) {
      int left =
          pbl_shared_cbuf_get_read_space_remaining(&s_buffer, &state->buffer_client.buffer_client);
      if (left != 0) {
        empty = false;
        break;
      }
      state = (AccelManagerState *)state->list_node.next;
    }
  }
  pbl_sensor_unlock(ACCEL);
  return empty;
}

static void prv_batch_cb(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                         void *user_data) {
  int32_t ug[3];

  if (!s_enabled || batch->num_frames == 0) {
    return;
  }

  const int64_t offset_ms = prv_realtime_offset_ms();
  const uint16_t num_frames = batch->num_frames;

  // The most recent sample is the last one in the batch.
  if (pbl_sensor_decode(sensor, batch, PBL_SENSOR_ACCEL, num_frames - 1, 1, ug) < 0) {
    return;
  }
  prv_make_accel_data(ug, offset_ms + batch->timestamp_us / 1000, &s_last_accel_data);

  PBL_ANALYTICS_ADD(accel_sample_count, num_frames);

  if (!s_buffer.clients) {
    return; // no clients so don't buffer any data
  }

  if (prv_shared_buffer_empty()) {
    s_last_empty_timestamp_ms = offset_ms + pbl_sensor_frame_time(batch, 0) / 1000;
  }

  // Reserve a contiguous run for the whole batch, then fill it in place. Falling
  // behind triggers an eviction pass, matching the per-sample write path.
  const uint16_t total = num_frames * sizeof(AccelManagerBufferData);
  uint8_t *seg1, *seg2;
  uint16_t seg1_length;
  bool rv = pbl_shared_cbuf_write_reserve(&s_buffer, total, false /*advance_slackers*/, &seg1,
                                          &seg1_length, &seg2);
  if (!rv) {
    PBL_LOG_WRN("Accel subscriber fell behind, truncating data");
    rv = pbl_shared_cbuf_write_reserve(&s_buffer, total, true /*advance_slackers*/, &seg1,
                                       &seg1_length, &seg2);
  }
  PBL_ASSERTN(rv);

  // write_index stays aligned to sizeof(AccelManagerBufferData), so a wrap never
  // splits a sample; seg1 holds the first seg1_items, seg2 the remainder.
  AccelManagerBufferData *dst = (AccelManagerBufferData *)seg1;
  const uint16_t seg1_items = seg1_length / sizeof(AccelManagerBufferData);
  for (uint16_t i = 0U; i < num_frames; ++i) {
    if (i == seg1_items) {
      dst = (AccelManagerBufferData *)seg2;
    }
    uint64_t ts_ms = offset_ms + pbl_sensor_frame_time(batch, i) / 1000;
    (void)pbl_sensor_decode(sensor, batch, PBL_SENSOR_ACCEL, i, 1, ug);
    dst->rawdata.x = (int16_t)(ug[0] / 1000);
    dst->rawdata.y = (int16_t)(ug[1] / 1000);
    dst->rawdata.z = (int16_t)(ug[2] / 1000);
    // Note: the delta value overflows if the s_buffer is not drained for ~65s,
    // but there should be more than enough time for it to drain in that window
    dst->timestamp_delta_ms = (uint16_t)(ts_ms - s_last_empty_timestamp_ms);
    ++dst;
  }

  pbl_shared_cbuf_write_commit(&s_buffer, total);

  prv_dispatch_data(true /* post_event */);
}

// Motor spin-down plus hardware shake-detection latency after the last vibe
// pulse during which a "shake" is almost certainly the motor, not the user.
// Also bridges the brief off-gaps between segments of a multi-pulse pattern.
#define ACCEL_VIBE_SHAKE_HOLDOFF_MS (500)

// The vibe motor mechanically trips the accel's hardware wake-up/tap
// interrupt. Ignore shake/tap events while the motor is on or just after it
// stopped so a vibrating notification doesn't self-trigger a shake action.
static bool prv_shake_caused_by_vibe(void) {
  if (vibes_get_vibe_strength() != VIBE_STRENGTH_OFF) {
    return true;
  }
  return vibes_get_time_since_last_vibe_ms() < ACCEL_VIBE_SHAKE_HOLDOFF_MS;
}

static void prv_shake_detected(IMUCoordinateAxis axis, int32_t direction) {
  if (!s_enabled) {
    return;
  }

  if (prv_shake_caused_by_vibe()) {
    PBL_LOG_DBG("Ignoring vibe-induced shake");
    return;
  }

#if !defined(CONFIG_RECOVERY_FW)
  extern bool shell_prefs_get_accel_shake_log_info_enabled(void);
  if (shell_prefs_get_accel_shake_log_info_enabled()) {
    PBL_LOG_INFO("Shake detected; axis=%d, direction=%" PRId32, axis, direction);
  } else {
    PBL_LOG_DBG("Shake detected; axis=%d, direction=%" PRId32, axis, direction);
  }
#else
  PBL_LOG_DBG("Shake detected; axis=%d, direction=%" PRId32, axis, direction);
#endif

  PebbleEvent e = {
    .type = PEBBLE_ACCEL_SHAKE_EVENT,
    .accel_tap = {
      .axis = axis,
      .direction = direction,
    },
  };

  event_put(&e);

  PBL_ANALYTICS_ADD(accel_shake_count, 1);
}

static void prv_double_tap_detected(IMUCoordinateAxis axis, int32_t direction) {
  if (!s_enabled) {
    return;
  }

  if (prv_shake_caused_by_vibe()) {
    PBL_LOG_DBG("Ignoring vibe-induced tap");
    return;
  }

  PebbleEvent e = {
    .type = PEBBLE_ACCEL_DOUBLE_TAP_EVENT,
    .accel_tap = {
      .axis = axis,
      .direction = direction,
    },
  };

  event_put(&e);

  PBL_ANALYTICS_ADD(accel_double_tap_count, 1);
}

static void prv_event_cb(const struct pbl_sensor *sensor, const struct pbl_sensor_event *event,
                         void *user_data) {
  switch (event->type) {
    case PBL_SENSOR_EVENT_SHAKE:
      prv_shake_detected((IMUCoordinateAxis)event->axis, event->direction);
      break;
    case PBL_SENSOR_EVENT_DOUBLE_TAP:
      prv_double_tap_detected((IMUCoordinateAxis)event->axis, event->direction);
      break;
    default:
      break;
  }
}

static const struct pbl_sensor_listener s_listener = {
  .batch = prv_batch_cb,
  .event = prv_event_cb,
};
#if UNITTEST
/*
 * Helper routines strictly for unit tests
 */

void test_accel_manager_get_subsample_info(AccelManagerState *state, uint16_t *num, uint16_t *den,
                                           uint16_t *samps_per_update) {
  *num = state->buffer_client.numerator;
  *den = state->buffer_client.denominator;
  *samps_per_update = state->samples_per_update;
}

void test_accel_manager_reset(void) {
  s_buffer = (struct pbl_shared_cbuf){};
  AccelManagerState *state = (AccelManagerState *)s_data_subscribers;
  while (state) {
    AccelManagerState *free_state = state;
    state = (AccelManagerState *)state->list_node.next;
    pbl_free(free_state);
  }
  s_data_subscribers = nullptr;
  s_shake_subscribers_count = 0;
  s_double_tap_subscribers_count = 0;
  s_motion_backlight_subscribed = false;
  s_enabled = true;
  s_shake_sensitivity_percent = 100;
  s_shake_sensitivity_high = false;
}

#endif
