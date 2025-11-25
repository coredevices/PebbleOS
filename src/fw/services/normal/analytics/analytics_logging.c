/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include "applib/data_logging.h"
#include "comm/bt_lock.h"
#include "drivers/rtc.h"

#include "os/tick.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"

#include "services/normal/data_logging/data_logging_service.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/system_task.h"

#include "services/common/analytics/analytics_event.h"
#include "services/common/analytics/analytics_external.h"
#include "services/common/analytics/analytics_heartbeat.h"
#include "services/common/analytics/analytics_logging.h"
#include "services/common/analytics/analytics_metric.h"
#include "services/common/analytics/analytics_storage.h"
#include "services/common/system_task.h"

#include "system/logging.h"
#include "system/passert.h"

enum {
#ifdef ANALYTICS_DEBUG
  HEARTBEAT_INTERVAL = 10 * 1000, // 10 seconds
#else
  HEARTBEAT_INTERVAL = 60 * 60 * 1000, // 1 hour
#endif
};

static int s_heartbeat_timer;
static uint32_t s_previous_send_ticks;

DataLoggingSessionRef s_device_heartbeat_session = NULL;
DataLoggingSessionRef s_app_heartbeat_session = NULL;
DataLoggingSessionRef s_event_session = NULL;

static void prv_schedule_retry();
static void prv_create_event_session_cb(void *ignored);

static void prv_reset_local_session_ptrs(void) {
  s_device_heartbeat_session = NULL;
  s_app_heartbeat_session = NULL;
  s_event_session = NULL;
}

static void prv_timer_callback(void *data) {
  if (!dls_initialized()) {
    // We need to wait until data logging is initialized before we can log heartbeats
    prv_schedule_retry();
    return;
  }
  if (!s_event_session) {
    // If the event session has not been created yet, create that. The only time we may have to do
    // this here is if the first call to prv_create_event_session_cb() during boot failed to create
    // the session.
    launcher_task_add_callback(prv_create_event_session_cb, NULL);
  }
  system_task_add_callback(analytics_logging_system_task_cb, NULL);
  new_timer_start(s_heartbeat_timer, HEARTBEAT_INTERVAL, prv_timer_callback, NULL, 0);
}

static void prv_schedule_retry() {
  new_timer_start(s_heartbeat_timer, 5000, prv_timer_callback, NULL, 0);
}

static DataLoggingSessionRef prv_create_dls(AnalyticsBlobKind kind, uint32_t item_length) {
  Uuid system_uuid = UUID_SYSTEM;
  bool buffered = false;
  const char *kind_str;
  uint32_t tag;

  if (kind == ANALYTICS_BLOB_KIND_DEVICE_HEARTBEAT) {
    kind_str = "Device";
    tag = DlsSystemTagAnalyticsDeviceHeartbeat;
  } else if (kind == ANALYTICS_BLOB_KIND_APP_HEARTBEAT) {
    kind_str = "App";
    tag = DlsSystemTagAnalyticsAppHeartbeat;
  } else if (kind == ANALYTICS_BLOB_KIND_EVENT) {
    kind_str = "Event";
    buffered = true;
    tag = DlsSystemTagAnalyticsEvent;
  } else {
    WTF;
  }

  // TODO: Use different tag ids for device_hb and app_hb sessions.
  // https://pebbletechnology.atlassian.net/browse/PBL-5463
  const bool resume = false;
  DataLoggingSessionRef dls_session = dls_create(
      tag, DATA_LOGGING_BYTE_ARRAY, item_length, buffered, resume, &system_uuid);

  PBL_LOG(LOG_LEVEL_DEBUG, "%s HB Session: %p", kind_str, dls_session);
  if (!dls_session) {
    // Data logging full at boot. Reset it and try again 5s later
    PBL_LOG(LOG_LEVEL_WARNING, "Data logging full at boot. Clearing...");
    // We reset all data logging here, including data logging for applications,
    // because an inability to allocate a new session means all 200+ session
    // IDs are exhausted, likely caused by a misbehaving app_hb. See discussion at:
    // https://github.com/pebble/tintin/pull/1967#discussion-diff-11746345
    // And issue about removing/moving this at
    // https://pebbletechnology.atlassian.net/browse/PBL-5473
    prv_reset_local_session_ptrs();
    dls_clear();
    prv_schedule_retry();
    return NULL;
  }
  return dls_session;
}

static void prv_dls_log(AnalyticsHeartbeat *device_hb, AnalyticsHeartbeatList *app_hbs) {
  dls_log(s_device_heartbeat_session, device_hb->data, 1);
#ifdef ANALYTICS_DEBUG
  analytics_heartbeat_print(device_hb);
#endif
  kernel_free(device_hb);

  AnalyticsHeartbeatList *app_hb_node = app_hbs;
  while (app_hb_node) {
    AnalyticsHeartbeat *app_hb = app_hb_node->heartbeat;
#ifdef ANALYTICS_DEBUG
    analytics_heartbeat_print(app_hb);
#endif
    dls_log(s_app_heartbeat_session, app_hb->data, 1);

    AnalyticsHeartbeatList *next = (AnalyticsHeartbeatList*)app_hb_node->node.next;
    kernel_free(app_hb);
    kernel_free(app_hb_node);
    app_hb_node = next;
  }
}

// System task callback used to prepare and log the heartbeats using dls_log().
void analytics_logging_system_task_cb(void *ignored) {
  if (!s_device_heartbeat_session) {
    uint32_t size = analytics_heartbeat_kind_data_size(ANALYTICS_HEARTBEAT_KIND_DEVICE);
    s_device_heartbeat_session = prv_create_dls(ANALYTICS_BLOB_KIND_DEVICE_HEARTBEAT, size);
    if (!s_device_heartbeat_session) return;
  }

  // Tell the watchdog timer that we are still awake. dls_create() could take up to 4 seconds if
  // we can't get the ispp send buffer
  system_task_watchdog_feed();

  if (!s_app_heartbeat_session) {
    uint32_t size = analytics_heartbeat_kind_data_size(ANALYTICS_HEARTBEAT_KIND_APP);
    s_app_heartbeat_session = prv_create_dls(ANALYTICS_BLOB_KIND_APP_HEARTBEAT, size);
    if (!s_app_heartbeat_session) return;
  }

  // Tell the watchdog timer that we are still awake. dls_create() could take up to 4 seconds if
  // we can't get the ispp send buffer
  system_task_watchdog_feed();

  analytics_external_update();

  // Tell the watchdog timer that we are still awake. Occasionally, analytics_external_update()
  // could take a while to execute if it needs to wait for the bt_lock() for example
  system_task_watchdog_feed();

  // The phone and proxy server expect us to send local time. The phone will imbed the time zone
  // offset into the blob and the proxy server will then use that to convert to UTC before it
  // gets placed into the database
  uint32_t timestamp = time_utc_to_local(rtc_get_time());
  uint64_t current_ticks = rtc_get_ticks();

  AnalyticsHeartbeat *device_hb = NULL;
  AnalyticsHeartbeatList *app_hbs = NULL;

  {
    analytics_storage_take_lock();

    extern void analytics_stopwatches_update(uint64_t current_ticks);
    analytics_stopwatches_update(current_ticks);

    // Hijack the device_hb and app_hb heartbeats from analytics_storage.
    // After this point, we own the memory, so analytics_storage will not
    // modify it anymore. Thus, we do not need to hold the lock while
    // logging.
    device_hb = analytics_storage_hijack_device_heartbeat();
    app_hbs = analytics_storage_hijack_app_heartbeats();

    analytics_storage_give_lock();
  }

  uint32_t dt_ticks = current_ticks - s_previous_send_ticks;
  uint32_t dt_ms = ticks_to_milliseconds(dt_ticks);
  s_previous_send_ticks = current_ticks;

  analytics_heartbeat_set(device_hb, ANALYTICS_DEVICE_METRIC_TIMESTAMP, timestamp);
  analytics_heartbeat_set(device_hb, ANALYTICS_DEVICE_METRIC_DEVICE_UP_TIME, current_ticks);
  analytics_heartbeat_set(device_hb, ANALYTICS_DEVICE_METRIC_TIME_INTERVAL, dt_ms);

  AnalyticsHeartbeatList *app_hb_node = app_hbs;
  while (app_hb_node) {
    AnalyticsHeartbeat *app_hb = app_hb_node->heartbeat;
    analytics_heartbeat_set(app_hb, ANALYTICS_APP_METRIC_TIMESTAMP, timestamp);
    analytics_heartbeat_set(app_hb, ANALYTICS_APP_METRIC_TIME_INTERVAL, dt_ms);
    app_hb_node = (AnalyticsHeartbeatList*)app_hb_node->node.next;
  }

  prv_dls_log(device_hb, app_hbs);
}


// Launcher task callback used to create our event logging session.
static void prv_create_event_session_cb(void *ignored) {
  if (!s_event_session) {
    s_event_session = prv_create_dls(ANALYTICS_BLOB_KIND_EVENT, sizeof(AnalyticsEventBlob));
    // If the above call fails, it will schedule our timer to try again in a few seconds
  }
}

static void prv_handle_log_event(AnalyticsEventBlob *event_blob) {
  if (!s_event_session) {
    PBL_LOG(LOG_LEVEL_INFO, "Event dropped because session not created yet");
    return;
  }

  // Log it
  dls_log(s_event_session, event_blob, 1);
}

static void prv_handle_async_event_logging(void *data) {
  AnalyticsEventBlob *event_blob = (AnalyticsEventBlob *)data;
  prv_handle_log_event(event_blob);
  kernel_free(event_blob);
}

void analytics_logging_log_event(AnalyticsEventBlob *event_blob) {
  // Fill in the meta info
  event_blob->kind = ANALYTICS_BLOB_KIND_EVENT;
  event_blob->version = ANALYTICS_EVENT_BLOB_VERSION;
  event_blob->timestamp = time_utc_to_local(rtc_get_time());

  // TODO: We should be able to remove this once PBL-23925 is fixed
  if (bt_lock_is_held()) {
    // We run the risk of deadlocking if we hold the bt_lock at this point. If
    // it's the case then schedule a callback so the dls code runs while we no
    // longer hold the lock
    AnalyticsEventBlob *event_blob_copy =
        kernel_malloc_check(sizeof(AnalyticsEventBlob));
    memcpy(event_blob_copy, event_blob, sizeof(*event_blob_copy));
    system_task_add_callback(prv_handle_async_event_logging, event_blob_copy);
  } else {
    prv_handle_log_event(event_blob);
  }
}

void analytics_logging_init(void) {
  s_heartbeat_timer = new_timer_create();
  s_previous_send_ticks = rtc_get_ticks();
  new_timer_start(s_heartbeat_timer, HEARTBEAT_INTERVAL, prv_timer_callback, NULL, 0);

  // Create the event session on a launcher task callback because we have to wait for
  // services (like data logging service) to be initialized)
  launcher_task_add_callback(prv_create_event_session_cb, NULL);
}
