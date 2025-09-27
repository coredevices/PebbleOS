/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "os/mutex.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_management/worker_manager.h"

#include "services/common/analytics/analytics.h"
#include "services/common/analytics/analytics_metric.h"
#include "services/common/analytics/analytics_storage.h"
#include "system/logging.h"
#include "system/passert.h"

#if MEMFAULT
#include "memfault/components.h"
#endif

static PebbleRecursiveMutex *s_analytics_storage_mutex = NULL;

static AnalyticsHeartbeat *s_device_heartbeat = NULL;
static AnalyticsHeartbeatList *s_app_heartbeat_list = NULL;

#define MAX_APP_HEARTBEATS    8

void analytics_storage_init(void) {
  s_analytics_storage_mutex = mutex_create_recursive();
  PBL_ASSERTN(s_analytics_storage_mutex);
  s_device_heartbeat = analytics_heartbeat_device_create();
  PBL_ASSERTN(s_device_heartbeat);
}

//////////
// Lock
void analytics_storage_take_lock(void) {
#if MEMFAULT
  // Memfault can reenter into the analytics module (for instance, through
  // flash logging).  If it does, another thread that is performing an
  // analytics operation could hold-and-wait on analytics lock -> memfault
  // lock; if that happens, we'd deadlock.  Enforce an ordering on this.
  memfault_lock();
#endif
  mutex_lock_recursive(s_analytics_storage_mutex);
}

bool analytics_storage_has_lock(void) {
  bool has_lock = mutex_is_owned_recursive(s_analytics_storage_mutex);
  if (!has_lock) {
    PBL_LOG(LOG_LEVEL_ERROR, "Analytics lock is not held when it should be!");
  }
  return has_lock;
}

void analytics_storage_give_lock(void) {
  mutex_unlock_recursive(s_analytics_storage_mutex);
#if MEMFAULT
  memfault_unlock();
#endif
}

///////
// Get
AnalyticsHeartbeat *analytics_storage_hijack_device_heartbeat() {
  PBL_ASSERTN(analytics_storage_has_lock());

  AnalyticsHeartbeat *device = s_device_heartbeat;

  s_device_heartbeat = analytics_heartbeat_device_create();
  PBL_ASSERTN(s_device_heartbeat);

  return device;
}

AnalyticsHeartbeatList *analytics_storage_hijack_app_heartbeats() {
  PBL_ASSERTN(analytics_storage_has_lock());

  AnalyticsHeartbeatList *apps = s_app_heartbeat_list;
  s_app_heartbeat_list = NULL;
  return apps;
}

///////////
// Search
static bool prv_is_app_node_with_uuid(ListNode *found_node, void *data) {
  const Uuid *searching_for_uuid = (const Uuid*)data;
  AnalyticsHeartbeatList *app_node = (AnalyticsHeartbeatList*)found_node;
  const Uuid *found_uuid = analytics_heartbeat_get_uuid(app_node->heartbeat);
  return uuid_equal(searching_for_uuid, found_uuid);
}

static AnalyticsHeartbeatList *prv_app_node_create(const Uuid *uuid) {
  AnalyticsHeartbeatList *app_heartbeat_node = kernel_malloc_check(sizeof(AnalyticsHeartbeatList));

  list_init(&app_heartbeat_node->node);
  app_heartbeat_node->heartbeat = analytics_heartbeat_app_create(uuid);

  return app_heartbeat_node;
}

const Uuid *analytics_uuid_for_client(AnalyticsClient client) {
  const PebbleProcessMd *md;
  if (client == AnalyticsClient_CurrentTask) {
    PebbleTask task = pebble_task_get_current();
    if (task == PebbleTask_App) {
      client = AnalyticsClient_App;
    } else if (task == PebbleTask_Worker) {
      client = AnalyticsClient_Worker;
    } else {
      return NULL; // System UUID
    }
  }

  if (client == AnalyticsClient_App) {
    md = app_manager_get_current_app_md();
  } else if (client == AnalyticsClient_Worker) {
    md = worker_manager_get_current_worker_md();
  } else if (client == AnalyticsClient_System) {
    return NULL;
  } else if (client == AnalyticsClient_Ignore) {
    return NULL;
  } else {
    WTF;
  }

  if (md != NULL) {
    return &md->uuid;
  } else {
    return NULL;
  }
}


AnalyticsHeartbeat *analytics_storage_find(AnalyticsMetric metric, const Uuid *uuid,
                                           AnalyticsClient client) {
  PBL_ASSERTN(analytics_storage_has_lock());

  switch (analytics_metric_kind(metric)) {
  case ANALYTICS_METRIC_KIND_DEVICE:
    PBL_ASSERTN(client == AnalyticsClient_Ignore || client == AnalyticsClient_System);
    return s_device_heartbeat;
  case ANALYTICS_METRIC_KIND_APP: {
    PBL_ASSERTN(client == AnalyticsClient_Ignore || client != AnalyticsClient_System);
    const Uuid uuid_system = UUID_SYSTEM;
    if (!uuid) {
      uuid = analytics_uuid_for_client(client);
      if (!uuid) {
        // There is a brief period of time where no app is running, which we
        // attribute to the system UUID. For now, this lets us track how
        // much time we are missing, although we probably want to try and
        // tighten this up as much as possible going forward.
        uuid = &uuid_system;
      }
    }
    ListNode *node = list_find((ListNode*)s_app_heartbeat_list,
                               prv_is_app_node_with_uuid, (void*)uuid);
    AnalyticsHeartbeatList *app_node = (AnalyticsHeartbeatList*)node;
    if (!app_node) {
      if (list_count((ListNode *)s_app_heartbeat_list) >= MAX_APP_HEARTBEATS) {
        ANALYTICS_LOG_DEBUG("No more app heartbeat sessions available");
        return NULL;
      }
      app_node = prv_app_node_create(uuid);
      s_app_heartbeat_list = (AnalyticsHeartbeatList*)list_prepend(
        (ListNode*)s_app_heartbeat_list, &app_node->node);
    }
    return app_node->heartbeat;
  }
  default:
    WTF;
  }
}
