/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services_normal.h"

#include <string.h>

#include "applib/event_service_client.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "process_management/app_install_manager.h" // FIXME: This should really be in services/
#include "process_management/launcher_app_message.h" // FIXME: This should really be in services/
#include "services/normal/activity/activity.h"
#include "services/normal/alarms/alarm.h"
#include "services/normal/app_cache.h"
#include "services/normal/app_fetch_endpoint.h"
#include "services/normal/app_glances/app_glance_service.h"
#include "services/normal/blob_db/api.h"
#include "services/normal/blob_db/endpoint_private.h"
#include "services/normal/data_logging/data_logging_service.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/protobuf_log/protobuf_log.h"
#include "services/normal/music_endpoint.h"
#include "services/normal/music_internal.h"
#include "services/normal/notifications/alerts_private.h"
#include "services/normal/notifications/notifications.h"
#include "services/normal/persist.h"
#include "services/normal/phone_call.h"
#include "services/normal/process_management/app_order_storage.h"
#include "services/normal/send_text_service.h"
#include "services/normal/stationary.h"
#include "services/normal/timeline/event.h"
#include "services/normal/wakeup.h"
#include "services/normal/weather/weather_service.h"
#include "services/runlevel_impl.h"

#if CAPABILITY_HAS_ORIENTATION_MANAGER
#include "services/normal/orientation_manager.h"
#endif

#include "services/normal/activity/activity.h"
#include "services/normal/voice/voice.h"

#include "util/size.h"

// Minimum valid time: January 1, 2010 00:00:00 UTC (timestamp: 1262304000)
// This represents the minimum time we consider valid for activity initialization
#define MIN_VALID_TIME_TIMESTAMP 1262304000

// State for deferred activity initialization
static bool s_activity_init_deferred = false;
static EventServiceInfo s_time_event_info;

static void prv_time_set_event_handler(PebbleEvent *e, void *context) {
  // Check if time is now valid and activity init was deferred
  if (s_activity_init_deferred && rtc_get_time() >= MIN_VALID_TIME_TIMESTAMP) {
    // Time is now valid, initialize activity
    s_activity_init_deferred = false;
    
    // Unsubscribe from time events
    event_service_client_unsubscribe(&s_time_event_info);
    
    activity_init();
    // If the user had tracking enabled before init was deferred, start tracking now so we
    // don't miss steps when initialization happens after boot.
    if (activity_prefs_tracking_is_enabled()) {
      // Ensure the runlevel-enabled flag is set for the activity service so the usual
      // enable/disable machinery will attempt to start tracking. This covers the case
      // where services_set_runlevel() already ran before activity was initialized.
      activity_set_enabled(true);
      activity_start_tracking(false /* test_mode */);
    }
  }
}

static bool prv_is_time_valid_for_activity_init(void) {
  return rtc_get_time() >= MIN_VALID_TIME_TIMESTAMP;
}

void services_normal_early_init(void) {
  pfs_init(true);
}

void services_normal_init(void) {
  persist_service_init();

  app_install_manager_init();

  blob_db_init_dbs();
  app_cache_init();
  phone_call_service_init();
  music_init();
  alarm_init();
  timeline_event_init();
  dls_init();

  wakeup_init();

  app_order_storage_init();

#if CAPABILITY_HAS_HEALTH_TRACKING
  // Check if time is valid before initializing activity
  if (prv_is_time_valid_for_activity_init()) {
    activity_init();
  } else {
    // Defer activity initialization until time is set properly
    s_activity_init_deferred = true;
    
    // Subscribe to time set events
    s_time_event_info = (EventServiceInfo) {
      .type = PEBBLE_SET_TIME_EVENT,
      .handler = prv_time_set_event_handler,
    };
    event_service_client_subscribe(&s_time_event_info);
  }
#endif

  notifications_init();
  alerts_init();
  send_text_service_init();
  protobuf_log_init();

#if CAPABILITY_HAS_WEATHER
  weather_service_init();
#endif

#if CAPABILITY_HAS_MICROPHONE
  voice_init();
#endif

#if CAPABILITY_HAS_APP_GLANCES
  app_glance_service_init();
#endif
}

static struct ServiceRunLevelSetting s_runlevel_settings[] = {
  {
    .set_enable_fn = wakeup_enable,
    .enable_mask = R_Stationary | R_Normal,
  },
  {
    .set_enable_fn = alarm_service_enable_alarms,
    .enable_mask = R_LowPower | R_Stationary | R_Normal,
  },
#if CAPABILITY_HAS_HEALTH_TRACKING
  {
    .set_enable_fn = activity_set_enabled,
    .enable_mask = R_Stationary | R_Normal,
  },
#endif
  {
    .set_enable_fn = stationary_run_level_enable,
    .enable_mask = R_Stationary | R_Normal,
  },
  {
    .set_enable_fn = dls_set_send_enable_run_level,
    .enable_mask = R_Normal,
  },
  {
    .set_enable_fn = blob_db_enabled,
    .enable_mask = R_Normal,
  },
#if CAPABILITY_HAS_ORIENTATION_MANAGER
  {
    .set_enable_fn = orientation_manager_enable,
    .enable_mask = R_LowPower | R_Stationary | R_Normal,
  }
#endif
};

void services_normal_set_runlevel(RunLevel runlevel) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_runlevel_settings); ++i) {
    struct ServiceRunLevelSetting *service = &s_runlevel_settings[i];
    service->set_enable_fn(((1 << runlevel) & service->enable_mask) != 0);
  }
}
