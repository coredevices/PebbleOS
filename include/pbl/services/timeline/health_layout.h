/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "item.h"
#include "layout_layer.h"
#include "timeline_layout.h"

#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_timeline_health_layout Health layout
 * @ingroup services_timeline
 * @brief Layout of health pins (LayoutIdHealth) and Health app launch arguments.
 * @{
 */

/** @brief Size of a health metric text buffer, in bytes. */
#define HEALTH_METRIC_BUFFER_LENGTH 128

/** @brief Health app cards. */
typedef enum HealthCardType {
  /** Activity summary. */
  HealthCardType_Activity = 0,
  /** Sleep summary. */
  HealthCardType_Sleep,
  /** Number of card types. */
  HealthCardTypeCount
} HealthCardType;

/** @brief Launch arguments of the Health app, shared with insights to open the right card. */
typedef struct PBL_PACKED HealthLaunchArgs {
  /** Arguments. */
  union {
    /** Decoded arguments. */
    struct {
      /** Card to open. */
      HealthCardType card_type : 8;
    };
    /** Raw launch argument value. */
    uint32_t args;
  };
} HealthLaunchArgs;

/** @brief Metrics of an activity session. */
typedef enum {
  /** Duration. */
  ActivitySessionMetric_Duration = 0,
  /** Calories. */
  ActivitySessionMetric_Calories,
  /** Distance. */
  ActivitySessionMetric_Distance,
  /** Pace. */
  ActivitySessionMetric_Pace,
  /** Number of metrics. */
  ActivitySessionMetricCount,
} ActivitySessionMetric;

/** @brief Display state of a metric. */
typedef struct {
  /** Icon of the metric. */
  KinoLayer *icon_layer;
  /** Format string of the value. */
  const char *value_fmt;
} MetricData;

/** @brief Health pin layout. */
typedef struct {
  /** Base timeline layout. */
  TimelineLayout timeline_layout;
  /** Display state of each activity session metric. */
  MetricData metric_data[ActivitySessionMetricCount];
} HealthLayout;

/**
 * @brief Create a health layout.
 *
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *health_layout_create(const LayoutLayerConfig *config);

/**
 * @brief Check the attributes of a health pin.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if a title is present.
 */
bool health_layout_verify(bool existing_attributes[]);

/** @} */
