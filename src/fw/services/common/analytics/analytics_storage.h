/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "analytics.h"
#include "kernel/pebble_tasks.h"
#include "analytics_heartbeat.h"

typedef struct {
  ListNode node;
  AnalyticsHeartbeat *heartbeat;
} AnalyticsHeartbeatList;

typedef void (*AnalyticsHeartbeatCallback)(AnalyticsHeartbeat *heartbeat, void *data);

void analytics_storage_init(void);

void analytics_storage_take_lock(void);
bool analytics_storage_has_lock(void);
void analytics_storage_give_lock(void);

// Must hold the lock before using any of the functions below this marker.
AnalyticsHeartbeat *analytics_storage_hijack_device_heartbeat();
AnalyticsHeartbeatList *analytics_storage_hijack_app_heartbeats();

AnalyticsHeartbeat *analytics_storage_find(AnalyticsMetric metric, const Uuid *uuid,
                                           AnalyticsClient client);

const Uuid *analytics_uuid_for_client(AnalyticsClient client);
