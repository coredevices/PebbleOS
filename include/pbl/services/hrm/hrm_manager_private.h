/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/msgq.h>
#include "hrm_manager.h"

#include <applib/event_service_client.h>
#include <pbl/services/hrm/hrm_activity_scene.h>
#include <pbl/drivers/rtc.h>
#include <kernel/events.h>
#include <pbl/kernel/mutex.h>
#include <process_management/app_install_types.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/util/list.h>
#include <pbl/util/circular_buffer.h>

#include <stdint.h>

/**
 * @defgroup services_hrm_hrm_manager_private HRM manager internals
 * @ingroup services_hrm
 * @brief Manager state, tuning constants and kernel subscriptions.
 * @{
 */

/**
 * @brief Callback receiving HRM events, for KernelBG and KernelMain subscribers.
 *
 * Called on KernelBG with the HRM manager lock held.
 *
 * @param event Event.
 * @param context Context given at subscription.
 */
typedef void (*HRMSubscriberCallback)(PebbleHRMEvent *event, void *context);

/**
 * @brief Time needed for a good reading after turning the sensor on, in seconds.
 *
 * The sensor is turned on this early for a subscriber that becomes due, and it is subtracted
 * from every subscriber's remaining time: a subscriber with an interval within it is always due
 * and keeps the sensor on, rather than paying an algorithm restart every interval.
 */
#define HRM_SENSOR_SPIN_UP_SEC 20

/**
 * @brief Maximum interval of a foreground app getting the low latency FIFO cadence, in seconds.
 *
 * Such an app shows live readings; slower subscribers get the default, cheaper cadence.
 */
#define HRM_LOW_LATENCY_MAX_INTERVAL_S 2

/** @brief Opaque accelerometer service state. */
typedef struct AccelServiceState AccelServiceState;

/** @brief Subscriber state. */
typedef struct HRMSubscriberState {
  /** Subscriber list node. */
  ListNode list_node;
  /** Session reference of this subscriber. */
  HRMSessionRef session_ref;
  /** App install id, @c INSTALL_ID_INVALID for system subscribers. */
  AppInstallId app_id;
  /** Task of the subscriber. */
  PebbleTask task;
  /** Queue events are sent to; NULL for KernelBG. */
  struct pbl_msgq *queue;

  /** Callback, only for KernelBG subscribers. */
  HRMSubscriberCallback callback_handler;
  /** Callback context, only for KernelBG subscribers. */
  void *callback_context;

  /** Requested update interval, in seconds. */
  uint32_t update_interval_s;
  /** Expiration time, UTC; 0 for never. */
  time_t expire_utc;
  /** A @c HRMEvent_SubscriptionExpiring event was sent. */
  bool sent_expiration_event;
  /**
   * Needs the prompt FIFO cadence (a foreground app showing live readings), rather than the
   * background one.
   */
  bool low_latency;
  /** Requested features. */
  HRMFeature features;

  /** Tick count of the last usable reading this subscriber received. */
  RtcTicks last_valid_bpm_ticks;
} HRMSubscriberState;

/**
 * @brief Number of events queued for KernelBG subscribers.
 *
 * Updates come at about 1 Hz, with up to 2 events per second: 8 events allow a 4 s stall when
 * subscribed to both BPM and another feature.
 */
#define NUM_EVENTS_TO_QUEUE (8)
/** @brief Size of the KernelBG event storage, in bytes. */
#define EVENT_STORAGE_SIZE (sizeof(PebbleHRMEvent) * NUM_EVENTS_TO_QUEUE)

/** @brief Accelerometer samples per accelerometer manager update. */
#define HRM_MANAGER_ACCEL_MANAGER_SAMPLES_PER_UPDATE 4

/**
 * @brief Number of driver reports between checks for turning the sensor off.
 *
 * Kept low so a served subscriber does not keep the LED lit, and the other optical path
 * blocked, for long.
 */
#define HRM_CHECK_SENSOR_DISABLE_COUNT 3

/** @brief Consecutive sensor enable failures after which retries stop until reboot. */
#define HRM_MAX_ENABLE_FAILURES 3

/**
 * @brief Sensor on-time after which a subscriber without a usable reading is deferred, in seconds.
 *
 * The subscriber then waits for its next interval instead of holding the sensor on. Well above
 * a normal serve cycle (spin-up plus a few seconds), well below the battery impact threshold.
 */
#define HRM_MAX_UNSERVED_TIME_SEC 120

/** @brief HRM manager state. */
struct HRMManagerState {
  /** Lock protecting the state. */
  struct pbl_mutex lock;
  /** Subscriber list. */
  ListNode *subscribers;

  /** Events pending for KernelBG subscribers. */
  CircularBuffer system_task_event_buffer;
  /** Number of dropped events. */
  uint32_t dropped_events;
  /** Last session reference assigned. */
  HRMSessionRef next_session_ref;
  /** Storage of @ref system_task_event_buffer. */
  uint8_t system_task_event_storage[EVENT_STORAGE_SIZE];

  /** Accelerometer manager subscription, while the sensor is on. */
  AccelManagerState *accel_state;
  /** Accelerometer manager sample buffer. */
  AccelRawData accel_manager_buffer[HRM_MANAGER_ACCEL_MANAGER_SAMPLES_PER_UPDATE];
  /** Lock protecting @ref accel_data. */
  struct pbl_mutex accel_data_lock;
  /** Accelerometer samples for the driver. */
  HRMAccelData accel_data;

  /** Battery state subscription, to track the charger. */
  EventServiceInfo charger_subscription;

  /** Timer turning the sensor back on. */
  TimerID update_enable_timer_id;

  /** Driver reports since the last turn-off check, up to @ref HRM_CHECK_SENSOR_DISABLE_COUNT. */
  uint8_t check_disable_counter;
  /** Consecutive enable failures, retries stop at @ref HRM_MAX_ENABLE_FAILURES. */
  uint8_t enable_failure_count;

  /** Features the sensor was last enabled with. */
  HRMFeature enabled_features;

  /**
   * Tick count when the sensor, or the current optical path, was turned on; 0 while off.
   */
  RtcTicks sensor_on_since_ticks;
  /** The unserved timeout warning was logged during this on-time. */
  bool unserved_timeout_logged;

  /** The run level allows the sensor. */
  bool enabled_run_level;
  /** The watch is not charging. */
  bool enabled_charging_state;

  /**
   * Features being sampled, 0 when off. Only one optical path, green (BPM, HRV) or red/IR
   * (SpO2), runs at a time.
   */
  HRMFeature active_features;

  /** Activity scene for the heart rate algorithm, re-applied at every sensor power on. */
  HRMActivityScene activity_scene;
};

/**
 * @brief Subscribe a KernelBG or KernelMain client to sensor updates.
 *
 * KernelBG clients must pass a callback. KernelMain clients may instead receive events through
 * the event service. Apps and workers use sys_hrm_manager_app_subscribe().
 *
 * @param app_id App install id, @c INSTALL_ID_INVALID for system subscribers.
 * @param update_interval_s Requested update interval, in seconds.
 * @param expire_s Seconds after which the subscription expires, 0 for never.
 * @param features Requested features.
 * @param low_latency true for consumers showing live data that need prompt updates; false for
 * background logging and streaming, letting the sensor drain its FIFO less often to save power.
 * @param callback KernelBG callback.
 * @param context Context passed to @p callback.
 * @return Session reference.
 */
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context);

/**
 * @brief Set the activity scene the heart rate algorithm optimizes for.
 *
 * Applied immediately and again at every sensor power on. Callable from any task.
 *
 * @param scene Activity scene.
 */
void hrm_manager_set_activity_scene(HRMActivityScene scene);

/** @} */
