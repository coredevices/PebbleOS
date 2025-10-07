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

#include "services/common/analytics/analytics_heartbeat.h"
#include "services/common/analytics/analytics_metric.h"
#include "services/common/analytics/analytics_logging.h"

#include "system/logging.h"
#include "system/passert.h"
#include "kernel/pbl_malloc.h"
#include "util/math.h"

#include <inttypes.h>
#include <stdio.h>

uint32_t analytics_heartbeat_kind_data_size(AnalyticsHeartbeatKind kind) {
  AnalyticsMetric last = ANALYTICS_METRIC_INVALID;
  switch (kind) {
  case ANALYTICS_HEARTBEAT_KIND_DEVICE:
    last = ANALYTICS_DEVICE_METRIC_END - 1;
    break;
  case ANALYTICS_HEARTBEAT_KIND_APP:
    last = ANALYTICS_APP_METRIC_END - 1;
    break;
  }
  PBL_ASSERTN(last != ANALYTICS_METRIC_INVALID);
  return analytics_metric_offset(last) + analytics_metric_size(last);
}

/////////////////////
// Private
static bool prv_verify_kinds_match(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric) {
  AnalyticsMetricKind metric_kind = analytics_metric_kind(metric);
  if ((metric_kind == ANALYTICS_METRIC_KIND_DEVICE) &&
      (heartbeat->kind == ANALYTICS_HEARTBEAT_KIND_DEVICE)) {
    return true;
  } else if ((metric_kind == ANALYTICS_METRIC_KIND_APP) &&
      (heartbeat->kind == ANALYTICS_HEARTBEAT_KIND_APP)) {
    return true;
  } else {
    PBL_CROAK("Metric kind does not match heartbeat kind! %d %d", metric_kind, heartbeat->kind);
  }
}
static uint8_t *prv_heartbeat_get_location(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric) {
  prv_verify_kinds_match(heartbeat, metric);
  if (analytics_metric_is_array(metric)) {
    PBL_CROAK("Attempt to use integer value for array metric.");
  }
  return heartbeat->data + analytics_metric_offset(metric);
}
static uint8_t *prv_heartbeat_get_array_location(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric,
          uint32_t index) {
  prv_verify_kinds_match(heartbeat, metric);
  if (!analytics_metric_is_array(metric)) {
    PBL_CROAK("Attempt to use array value for integer metric.");
  }
  uint32_t len = analytics_metric_num_elements(metric);
  uint32_t element_size = analytics_metric_element_size(metric);
  if (index > len) {
    PBL_CROAK("Attempt to use array value at invalid index %" PRId32 " (len %" PRId32 ")",
              index, len);
  }
  return heartbeat->data + analytics_metric_offset(metric) + index*element_size;
}

static void prv_location_set_value(uint8_t *location, int64_t val, AnalyticsMetricElementType type) {
  switch (type) {
  case ANALYTICS_METRIC_ELEMENT_TYPE_NIL:
    WTF;
 case ANALYTICS_METRIC_ELEMENT_TYPE_UINT8:
   {
     *((uint8_t*)location) = (uint8_t)CLIP(val, 0, UINT8_MAX);
    return;
   }
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT16:
    {
      *((uint16_t*)location) = (uint16_t)CLIP(val, 0, UINT16_MAX);
    return;
    }
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT32:
    {
      *((uint32_t*)location) = (uint32_t)CLIP(val, 0, UINT32_MAX);
    return;
    }
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT8:
    {
      *((int8_t*)location) = (int8_t)CLIP(val, INT8_MIN, INT8_MAX);
    return;
    }
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT16:
    {
      *((int16_t*)location) = (int16_t)CLIP(val, INT16_MIN, INT16_MAX);
    return;
    }
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT32:
    {
      *((int32_t*)location) = (int32_t)CLIP(val, INT32_MIN, INT32_MAX);
    return;
    }
  }
  WTF; // Should not get here!
}
static int64_t prv_location_get_value(uint8_t *location, AnalyticsMetricElementType type) {
  switch (type) {
  case ANALYTICS_METRIC_ELEMENT_TYPE_NIL:
    WTF;
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT8:
    return *(uint8_t*)location;
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT16:
    return *(uint16_t*)location;
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT32:
    return *(uint32_t*)location;
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT8:
    return *(int8_t*)location;
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT16:
    return *(int16_t*)location;
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT32:
    return *(int32_t*)location;
  }
  WTF; // Should not get here!
}

//////////
// Set

void analytics_heartbeat_set(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, int64_t val) {
  uint8_t *location = prv_heartbeat_get_location(heartbeat, metric);
  prv_location_set_value(location, val, analytics_metric_element_type(metric));
}

void analytics_heartbeat_set_array(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, uint32_t index, int64_t val) {
  uint8_t *location = prv_heartbeat_get_array_location(heartbeat, metric, index);
  prv_location_set_value(location, val, analytics_metric_element_type(metric));
}

void analytics_heartbeat_set_entire_array(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, const void* data) {
  uint8_t *location = prv_heartbeat_get_array_location(heartbeat, metric, 0);
  uint32_t size = analytics_metric_size(metric);
  memcpy(location, data, size);
}

/////////
// Get
int64_t analytics_heartbeat_get(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric) {
  uint8_t *location = prv_heartbeat_get_location(heartbeat, metric);
  return prv_location_get_value(location, analytics_metric_element_type(metric));
}

int64_t analytics_heartbeat_get_array(AnalyticsHeartbeat *heartbeat, AnalyticsMetric metric, uint32_t index) {
  uint8_t *location = prv_heartbeat_get_array_location(heartbeat, metric, index);
  return prv_location_get_value(location, analytics_metric_element_type(metric));
}

const Uuid *analytics_heartbeat_get_uuid(AnalyticsHeartbeat *heartbeat) {
  return (const Uuid*)prv_heartbeat_get_array_location(heartbeat, ANALYTICS_APP_METRIC_UUID, 0);
}

///////////////////
// Create / Clear
AnalyticsHeartbeat *analytics_heartbeat_create(AnalyticsHeartbeatKind kind) {
  uint32_t size = sizeof(AnalyticsHeartbeat) + analytics_heartbeat_kind_data_size(kind);
  AnalyticsHeartbeat *heartbeat = kernel_malloc_check(size);
  heartbeat->kind = kind;
  analytics_heartbeat_clear(heartbeat);
  return heartbeat;
}

AnalyticsHeartbeat *analytics_heartbeat_device_create() {
  AnalyticsHeartbeat *hb = analytics_heartbeat_create(ANALYTICS_HEARTBEAT_KIND_DEVICE);
  analytics_heartbeat_set(hb, ANALYTICS_DEVICE_METRIC_BLOB_KIND,
                          ANALYTICS_BLOB_KIND_DEVICE_HEARTBEAT);
  analytics_heartbeat_set(hb, ANALYTICS_DEVICE_METRIC_BLOB_VERSION,
                          ANALYTICS_DEVICE_HEARTBEAT_BLOB_VERSION);
  return hb;
}

AnalyticsHeartbeat *analytics_heartbeat_app_create(const Uuid *uuid) {
  AnalyticsHeartbeat *hb = analytics_heartbeat_create(ANALYTICS_HEARTBEAT_KIND_APP);
  analytics_heartbeat_set_entire_array(hb, ANALYTICS_APP_METRIC_UUID, uuid);
  analytics_heartbeat_set(hb, ANALYTICS_APP_METRIC_BLOB_KIND,
                          ANALYTICS_BLOB_KIND_APP_HEARTBEAT);
  analytics_heartbeat_set(hb, ANALYTICS_APP_METRIC_BLOB_VERSION,
                          ANALYTICS_APP_HEARTBEAT_BLOB_VERSION);
  return hb;
}

void analytics_heartbeat_clear(AnalyticsHeartbeat *heartbeat) {
  AnalyticsHeartbeatKind kind = heartbeat->kind;
  uint32_t size = sizeof(AnalyticsHeartbeat) + analytics_heartbeat_kind_data_size(kind);
  memset(heartbeat, 0, size);
  heartbeat->kind = kind;
}

//////////////////
// Debug
#ifdef ANALYTICS_DEBUG
// Function to get the name of a macro given it's runtime value. (i.e. mapping
//   (1: "ANALYTICS_DEVICE_METRIC_MSG_ID"),
//   (2: "ANALYTICS_DEVICE_METRIC_VERSION"),
//   ...
// )
#define CASE(name, ...) case name: return #name;
static const char *prv_get_metric_name(AnalyticsMetric metric) {
  switch (metric) {
    ANALYTICS_METRIC_TABLE(CASE,CASE,CASE,,,,,,)
    default: return "";
  }
}
#undef CASE

static void prv_print_heartbeat(AnalyticsHeartbeat *heartbeat, AnalyticsMetric start, AnalyticsMetric end) {
  for (AnalyticsMetric metric = start + 1; metric < end; metric++) {
    const char *name = prv_get_metric_name(metric);
    if (!analytics_metric_is_array(metric)) {
      int64_t val = analytics_heartbeat_get(heartbeat, metric);
      if (val >= 0) {
        PBL_LOG(LOG_LEVEL_DEBUG, "%3" PRIu32 ": %s: %" PRIu32 " (0x%" PRIx32")",
            analytics_metric_offset(metric), name, (uint32_t)val, (uint32_t)val);
      } else {
        PBL_LOG(LOG_LEVEL_DEBUG, "%3" PRIu32 ": %s: %" PRId32 " (0x%" PRIx32")",
            analytics_metric_offset(metric), name, (int32_t)val, (int32_t)val);
      }
      continue;
    }
    const size_t BUF_LENGTH = 256;
    char buf[BUF_LENGTH];
    uint32_t written = 0;
    for (uint32_t i = 0; i < analytics_metric_num_elements(metric); i++) {
      if (written > BUF_LENGTH) {
        PBL_LOG(LOG_LEVEL_DEBUG, "print buffer overflow by %lu bytes",
            BUF_LENGTH - written);
        continue;
      }
      int64_t val = analytics_heartbeat_get_array(heartbeat, metric, i);
      const char *sep = (i == 0 ? "" : ", ");
      if (val >= 0) {
        written += snprintf(buf + written, BUF_LENGTH - written,
            "%s%" PRIu32 " (0x%" PRIx32 ")", sep, (uint32_t)val, (uint32_t)val);
      } else {
        written += snprintf(buf + written, BUF_LENGTH - written,
            "%s%" PRId32 " (0x%" PRIx32 ")", sep, (int32_t)val, (int32_t)val);
      }
    }
    PBL_LOG(LOG_LEVEL_DEBUG, "%3" PRIu32 ": %s: %s", analytics_metric_offset(metric), name, buf);
  }
}

void analytics_heartbeat_print(AnalyticsHeartbeat *heartbeat) {
  switch (heartbeat->kind) {
  case ANALYTICS_HEARTBEAT_KIND_DEVICE:
    PBL_LOG(LOG_LEVEL_DEBUG, "Device heartbeat:");
    prv_print_heartbeat(heartbeat, ANALYTICS_DEVICE_METRIC_START, ANALYTICS_DEVICE_METRIC_END);
    break;
  case ANALYTICS_HEARTBEAT_KIND_APP: {
    const Uuid *uuid = analytics_heartbeat_get_uuid(heartbeat);
    char uuid_buf[UUID_STRING_BUFFER_LENGTH];
    uuid_to_string(uuid, uuid_buf);
    PBL_LOG(LOG_LEVEL_DEBUG, "App heartbeat for %s:", uuid_buf);
    prv_print_heartbeat(heartbeat, ANALYTICS_APP_METRIC_START, ANALYTICS_APP_METRIC_END);
    break;
  }
  default:
    PBL_LOG(LOG_LEVEL_DEBUG, "Unable to print heartbeat: Unrecognized kind %d", heartbeat->kind);
  }
}
#else
void analytics_heartbeat_print(AnalyticsHeartbeat *heartbeat) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Turn on ANALYTICS_DEBUG to get heartbeat printing support.");
}
#endif
