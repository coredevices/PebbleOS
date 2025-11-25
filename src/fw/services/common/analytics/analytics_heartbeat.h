/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "util/uuid.h"

#include "analytics_metric_table.h"

typedef enum {
  ANALYTICS_HEARTBEAT_KIND_DEVICE = 0,
  ANALYTICS_HEARTBEAT_KIND_APP = 1,
} AnalyticsHeartbeatKind;

typedef struct {
  // Note that the first byte of data[] is also the kind of the heartbeat.
  // We could merge these into one, but I'm not sure what kind of code gcc
  // will generate when so many fields are unaligned, and I don't really want
  // to risk the codesize.
  AnalyticsHeartbeatKind kind;
  uint8_t data[0];
} AnalyticsHeartbeat;

uint32_t analytics_heartbeat_kind_data_size(AnalyticsHeartbeatKind kind);

void analytics_heartbeat_set(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, int64_t val);
void analytics_heartbeat_set_array(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, uint32_t index, int64_t val);
void analytics_heartbeat_set_entire_array(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, const void* data);

int64_t analytics_heartbeat_get(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric);
int64_t analytics_heartbeat_get_array(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, uint32_t index);
const Uuid *analytics_heartbeat_get_uuid(AnalyticsHeartbeat *heartbeat);

AnalyticsHeartbeat *analytics_heartbeat_device_create();
AnalyticsHeartbeat *analytics_heartbeat_app_create(const Uuid *uuid);
void analytics_heartbeat_clear(AnalyticsHeartbeat *heartbeat);

// Turning this on is helpful when debugging analytics subsystems. It changes the heartbeat
// to run once every 10 seconds instead of once every hour and also prints out the value of
// each metric. Also helpful is to change LOG_DOMAIN_ANALYTICS from 0 to 1 to enable extra
// logging messages (found in core/system/logging.h).
// Another useful debugging tip is that doing a long-select on any item in the launcher menu
// will trigger data logging to do an immediate flush of logged data to the phone.
// #define ANALYTICS_DEBUG

void analytics_heartbeat_print(AnalyticsHeartbeat *heartbeat);
