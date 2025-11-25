/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/analytics/analytics_metric.h"

#include "system/passert.h"
#include "util/size.h"

typedef struct {
  AnalyticsMetricElementType element_type;
  uint8_t num_elements;
} AnalyticsMetricDataType;

// http://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
#define GET_MACRO(_1, _2, _3, NAME, ...) NAME

#define ENTRY3(name, element_type, num_elements) {element_type, num_elements},
#define ENTRY2(name, element_type) {element_type, 1},
#define ENTRY1(name) {ANALYTICS_METRIC_ELEMENT_TYPE_NIL, 0},
#define ENTRY(...) GET_MACRO(__VA_ARGS__, ENTRY3, ENTRY2, ENTRY1)(__VA_ARGS__)

// Mapping from type index to data type of metric. We waste some space here
// by including the marker metrics, but it makes the code a fair bit simpler
// since we don't need an index translation table.
static const AnalyticsMetricDataType s_heartbeat_template[] = {
  ANALYTICS_METRIC_TABLE(ENTRY, ENTRY, ENTRY,
                         ANALYTICS_METRIC_ELEMENT_TYPE_UINT8,
                         ANALYTICS_METRIC_ELEMENT_TYPE_UINT16,
                         ANALYTICS_METRIC_ELEMENT_TYPE_UINT32,
                         ANALYTICS_METRIC_ELEMENT_TYPE_INT8,
                         ANALYTICS_METRIC_ELEMENT_TYPE_INT16,
                         ANALYTICS_METRIC_ELEMENT_TYPE_INT32)
};

#define NUM_METRICS ARRAY_LENGTH(s_heartbeat_template)

static const AnalyticsMetricDataType *prv_get_metric_data_type(AnalyticsMetric metric) {
  PBL_ASSERTN(analytics_metric_kind(metric) != ANALYTICS_METRIC_KIND_UNKNOWN);
  return &s_heartbeat_template[metric];
}

AnalyticsMetricElementType analytics_metric_element_type(AnalyticsMetric metric) {
  return prv_get_metric_data_type(metric)->element_type;
}

uint32_t analytics_metric_num_elements(AnalyticsMetric metric) {
  return prv_get_metric_data_type(metric)->num_elements;
}

uint32_t analytics_metric_element_size(AnalyticsMetric metric) {
  const AnalyticsMetricDataType *type = prv_get_metric_data_type(metric);
  switch (type->element_type) {
  case ANALYTICS_METRIC_ELEMENT_TYPE_NIL:
    return 0;
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT8:
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT8:
    return 1;
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT16:
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT16:
    return 2;
  case ANALYTICS_METRIC_ELEMENT_TYPE_UINT32:
  case ANALYTICS_METRIC_ELEMENT_TYPE_INT32:
    return 4;
  }
  PBL_CROAK("no such element_type %d", type->element_type);
}

uint32_t analytics_metric_size(AnalyticsMetric metric) {
  uint32_t num_elements = analytics_metric_num_elements(metric);
  uint32_t element_size = analytics_metric_element_size(metric);
  return num_elements * element_size;
}

bool analytics_metric_is_array(AnalyticsMetric metric) {
  const AnalyticsMetricDataType *type = prv_get_metric_data_type(metric);
  return (type->num_elements > 1);
}

bool analytics_metric_is_unsigned(AnalyticsMetric metric) {
  const AnalyticsMetricDataType *type = prv_get_metric_data_type(metric);
  return (type->element_type == ANALYTICS_METRIC_ELEMENT_TYPE_UINT32 ||
          type->element_type == ANALYTICS_METRIC_ELEMENT_TYPE_UINT16 ||
          type->element_type == ANALYTICS_METRIC_ELEMENT_TYPE_UINT8);
}

static uint16_t s_metric_heartbeat_offset[NUM_METRICS];

void analytics_metric_init(void) {
  uint32_t device_offset = 0;
  uint32_t app_offset = 0;
  const uint16_t INVALID_OFFSET = ~0;
  for (AnalyticsMetric metric = ANALYTICS_METRIC_START;
       metric < ANALYTICS_METRIC_END; metric++) {
    switch (analytics_metric_kind(metric)) {
    case ANALYTICS_METRIC_KIND_DEVICE:
      s_metric_heartbeat_offset[metric] = device_offset;
      device_offset += analytics_metric_size(metric);
      PBL_ASSERTN(device_offset < INVALID_OFFSET);
      break;
    case ANALYTICS_METRIC_KIND_APP:
      s_metric_heartbeat_offset[metric] = app_offset;
      app_offset += analytics_metric_size(metric);
      PBL_ASSERTN(app_offset < INVALID_OFFSET);
      break;
    case ANALYTICS_METRIC_KIND_MARKER:
      // Marker metrics do not actually exist in either heartbeat, they are
      // markers only.
      s_metric_heartbeat_offset[metric] = INVALID_OFFSET;
      break;
    case ANALYTICS_METRIC_KIND_UNKNOWN:
      WTF;
    }
  }
}

uint32_t analytics_metric_offset(AnalyticsMetric metric) {
  AnalyticsMetricKind kind = analytics_metric_kind(metric);
  PBL_ASSERTN((kind == ANALYTICS_METRIC_KIND_DEVICE) || (kind == ANALYTICS_METRIC_KIND_APP));
  return s_metric_heartbeat_offset[metric];
}

AnalyticsMetricKind analytics_metric_kind(AnalyticsMetric metric) {
  if ((metric > ANALYTICS_DEVICE_METRIC_START)
      && (metric < ANALYTICS_DEVICE_METRIC_END)) {
    return ANALYTICS_METRIC_KIND_DEVICE;
  } else if ((metric > ANALYTICS_APP_METRIC_START)
      && (metric < ANALYTICS_APP_METRIC_END)) {
    return ANALYTICS_METRIC_KIND_APP;
  } else if ((metric >= ANALYTICS_METRIC_START)
      && (metric <= ANALYTICS_METRIC_END)) {
    // "Marker" metrics are not actual real metrics, they are only used
    // to easily find the position of other metrics. (i.e. ANALYTICS_METRIC_START
    // is a "marker" metric).
    return ANALYTICS_METRIC_KIND_MARKER;
  } else {
    return ANALYTICS_METRIC_KIND_UNKNOWN;
  }
}
