/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/analytics/analytics.h>
#include <pbl/drivers/sensor.h>
#include <pbl/drivers/vibe.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/sensing/sensing.h>
#include <pbl/services/accel_manager.h>
#include <pbl/services/event_service.h>
#include <pbl/services/imu/units.h>
#include <pbl/services/vibe_pattern.h>
#include <pbl/util/list.h>
#include <pbl/util/math.h>

#include <kernel/events.h>
#include <syscall/syscall.h>

PBL_LOG_MODULE_DEFINE(service_accel_manager, CONFIG_SERVICE_ACCEL_MANAGER_LOG_LEVEL);

#define US_PER_SECOND (1000 * 1000)

// Frames read from the sensing subsystem at a time
#define READ_CHUNK 8U

// We create one of these for each data service subscriber
typedef struct AccelManagerState {
  ListNode list_node; // Entry into the s_data_subscribers linked list

  struct pbl_sensing_client client;
  //! The requested number of samples needed before calling data_cb_handler
  uint16_t samples_per_update;

  //! Which task we should call the data_cb_handler on
  PebbleTask task;
  CallbackEventCallback data_cb_handler;
  void *data_cb_context;

  uint64_t timestamp_ms;    // timestamp of first item in the buffer
  AccelRawData *raw_buffer; // raw buffer allocated by subscriber
  uint16_t num_samples;     // number of samples in raw_buffer
  bool event_posted;        // True while a data event is queued or running
  uint32_t generation;      // bumped on every buffer change
} AccelManagerState;

//! List of all registered consumers of accel data. Points to AccelManagerState objects.
static ListNode *s_data_subscribers = nullptr;

//! Reference count of how many shake subscribers we have. Used to turn off the feature when not
//! in use.
static uint8_t s_shake_subscribers_count = 0;
//! Reference count of how many double tap subscribers we have. Used to turn off the feature when
//! not in use.
static uint8_t s_double_tap_subscribers_count = 0;

//! Client for shake and double tap events
static void prv_event_cb(struct pbl_sensing_client *client, const struct pbl_sensor_event *event);
static struct pbl_sensing_client s_event_client;

//! Frames acquired, at the last analytics update
static uint32_t s_analytics_count;

//! Whether the kernel is subscribed to shake events for the motion backlight feature.
static bool s_motion_backlight_subscribed = false;

// Accel Idle
#define ACCEL_MAX_IDLE_DELTA 100
static AccelData s_last_analytics_position;
static AccelData s_last_accel_data;

//! Shake sensitivity set by the user preference, applied unless high sensitivity is on
static uint8_t s_shake_sensitivity_percent = 100;
static bool s_shake_sensitivity_high;

static uint32_t prv_get_max_batch(void) {
  int32_t val = 0;

  if (pbl_sensor_attr_get(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_BATCH_MAX, &val) != 0) {
    return 0;
  }

  return (uint32_t)val;
}

static void prv_apply_shake_sensitivity(void) {
  int32_t val = s_shake_sensitivity_high ? 100 : s_shake_sensitivity_percent;

  (void)pbl_sensor_attr_set(ACCEL, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_SHAKE_SENSITIVITY, &val);
}

//! Offset from uptime to the wall clock, in milliseconds
static int64_t prv_realtime_offset_ms(void) {
  return (int64_t)pbl_realtime_ms() - (int64_t)pbl_uptime_ms();
}

static void prv_update_events(void) {
  uint8_t events = 0U;

  if (s_shake_subscribers_count > 0) {
    events |= PBL_BIT(PBL_SENSOR_EVENT_SHAKE);
  }
  if (s_double_tap_subscribers_count > 0) {
    events |= PBL_BIT(PBL_SENSOR_EVENT_DOUBLE_TAP);
  }

  pbl_sensing_client_set_events(&s_event_client, events);
}

static void prv_shake_add_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);
  if (++s_shake_subscribers_count == 1) {
    PBL_LOG_DBG("Starting accel shake service");
    prv_update_events();
  }
  pbl_sensor_unlock(ACCEL);
}

static void prv_shake_remove_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);
  PBL_ASSERTN(s_shake_subscribers_count > 0);
  if (--s_shake_subscribers_count == 0) {
    PBL_LOG_DBG("Stopping accel shake service");
    prv_update_events();
  }
  pbl_sensor_unlock(ACCEL);
}

static void prv_double_tap_add_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);
  if (++s_double_tap_subscribers_count == 1) {
    PBL_LOG_DBG("Starting accel double tap service");
    prv_update_events();
  }
  pbl_sensor_unlock(ACCEL);
}

static void prv_double_tap_remove_subscriber_cb(PebbleTask task) {
  pbl_sensor_lock(ACCEL);
  PBL_ASSERTN(s_double_tap_subscribers_count > 0);
  if (--s_double_tap_subscribers_count == 0) {
    PBL_LOG_DBG("Stopping accel double tap service");
    prv_update_events();
  }
  pbl_sensor_unlock(ACCEL);
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

static void prv_update_analytics(void) {
  uint32_t count = pbl_sensing_count(ACCEL, PBL_SENSOR_ACCEL);

  PBL_ANALYTICS_ADD(accel_sample_count, count - s_analytics_count);
  s_analytics_count = count;
}

//! Fills the subscriber's buffer with the samples waiting for it
static void prv_fill(AccelManagerState *state) {
  int32_t ug[READ_CHUNK][3];

  prv_update_analytics();

  while (state->raw_buffer != nullptr && state->num_samples < state->samples_per_update) {
    uint16_t max =
        (uint16_t)PBL_MIN((uint32_t)(state->samples_per_update - state->num_samples), READ_CHUNK);
    uint64_t timestamp_us;
    uint16_t n = pbl_sensing_read(&state->client, &ug[0][0], max, &timestamp_us);

    if (n == 0U) {
      break;
    }

    if (state->num_samples == 0U) {
      state->timestamp_ms = prv_realtime_offset_ms() + timestamp_us / 1000U;
    }

    for (uint16_t i = 0U; i < n; i++) {
      state->raw_buffer[state->num_samples++] = (AccelRawData){
        .x = (int16_t)(ug[i][0] / 1000),
        .y = (int16_t)(ug[i][1] / 1000),
        .z = (int16_t)(ug[i][2] / 1000),
      };
    }

    s_last_accel_data.x = state->raw_buffer[state->num_samples - 1U].x;
    s_last_accel_data.y = state->raw_buffer[state->num_samples - 1U].y;
    s_last_accel_data.z = state->raw_buffer[state->num_samples - 1U].z;
  }
}

static void prv_arm(AccelManagerState *state) {
  uint16_t frames = 0U;

  if (state->raw_buffer != nullptr && !state->event_posted) {
    frames = state->samples_per_update - state->num_samples;
  }

  pbl_sensing_arm(&state->client, frames);
}

//! Called once the samples to fill the subscriber's buffer are in
static bool prv_notify(struct pbl_sensing_client *client) {
  AccelManagerState *state = client->user_data;

  prv_fill(state);

  if (state->num_samples < state->samples_per_update) {
    prv_arm(state);
    return true;
  }

  state->event_posted = prv_call_data_callback(state);
  if (!state->event_posted) {
    PBL_LOG_ERR("Failed to post accel event to task: 0x%x", (int)state->task);
  }

  return state->event_posted;
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

// Update the motion sensitivity based on user preference (0-100%), where higher is more
// sensitive: a lower wake-up threshold, for the accelerometers that support it
void accel_manager_update_sensitivity(uint8_t sensitivity_percent) {
  pbl_sensor_lock(ACCEL);
  s_shake_sensitivity_percent = PBL_MIN(sensitivity_percent, 100);
  prv_apply_shake_sensitivity();
  pbl_sensor_unlock(ACCEL);
}

void accel_manager_init(void) {
  s_event_client = (struct pbl_sensing_client){
    .sensor = ACCEL,
    .type = PBL_SENSOR_ACCEL,
    .event = prv_event_cb,
  };
  (void)pbl_sensing_client_add(&s_event_client);

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
      .client =
          {
            .sensor = ACCEL,
            .type = PBL_SENSOR_ACCEL,
            .interval_us = US_PER_SECOND / rate,
            .notify = prv_notify,
            .user_data = state,
          },
      .task = handler_task,
      .data_cb_handler = data_cb,
      .data_cb_context = context,
      .samples_per_update = prv_get_max_batch(),
    };

    bool no_subscribers_before = (s_data_subscribers == nullptr);
    s_data_subscribers = list_insert_before(s_data_subscribers, &state->list_node);
    if (no_subscribers_before) {
      sys_vibe_history_start_collecting();
    }

    // Samples are dropped until the subscriber sets a buffer
    int ret = pbl_sensing_client_add(&state->client);
    if (ret != 0) {
      PBL_LOG_ERR("Could not add sensing client (%d)", ret);
    }
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
  pbl_sensing_client_remove(&state->client);
  list_remove(&state->list_node, &s_data_subscribers /* &head */, nullptr /* &tail */);
  pbl_free(state);

  if (!s_data_subscribers) {
    // If no one left using the data subscription, disable it
    sys_vibe_history_stop_collecting();
  }
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

  pbl_sensing_client_set_interval(&state->client, US_PER_SECOND / rate);

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

  pbl_sensing_client_set_interval(&state->client,
                                  (US_PER_SECOND * 1000) / ONLY_SUPPORTED_JITTERFREE_RATE_MILLIHZ);

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
    pbl_sensing_client_set_batch(&state->client,
                                 (state->raw_buffer != nullptr) ? samples_per_update : 0U);
    prv_arm(state);
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

  // Fill it again from the samples waiting
  prv_fill(state);
  // A batch that's already in goes to the running callback, so no event is posted for it
  *more = state->samples_per_update > 0 && state->num_samples >= state->samples_per_update;
  state->event_posted = *more;
  prv_arm(state);

  pbl_sensor_unlock(ACCEL);
  return success;
}

void accel_manager_enable(bool on) {
  pbl_sensing_suspend(ACCEL, !on);
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
 * Events
 */

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

static void prv_event_cb(struct pbl_sensing_client *client, const struct pbl_sensor_event *event) {
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

#if UNITTEST
/*
 * Helper routines strictly for unit tests
 */

struct pbl_sensing_client *test_accel_manager_get_client(AccelManagerState *state) {
  return &state->client;
}

void test_accel_manager_reset(void) {
  AccelManagerState *state = (AccelManagerState *)s_data_subscribers;
  while (state) {
    AccelManagerState *free_state = state;
    state = (AccelManagerState *)state->list_node.next;
    pbl_sensing_client_remove(&free_state->client);
    pbl_free(free_state);
  }
  pbl_sensing_client_remove(&s_event_client);
  pbl_sensing_suspend(ACCEL, false);
  s_data_subscribers = nullptr;
  s_shake_subscribers_count = 0;
  s_double_tap_subscribers_count = 0;
  s_motion_backlight_subscribed = false;
  s_shake_sensitivity_percent = 100;
  s_shake_sensitivity_high = false;
}

#endif
