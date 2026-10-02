/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layout_layer.h"
#include "timeline_layout.h"

/**
 * @defgroup services_timeline_alarm_layout Alarm layout
 * @ingroup services_timeline
 * @brief Layout of alarm pins (LayoutIdAlarm).
 * @{
 */

/** @brief Alarm pin layout. */
typedef struct {
  /** Base timeline layout. */
  TimelineLayout timeline_layout;
} AlarmLayout;

/**
 * @brief Create an alarm layout.
 *
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *alarm_layout_create(const LayoutLayerConfig *config);

/**
 * @brief Check the attributes of an alarm pin.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if a title and a subtitle are present.
 */
bool alarm_layout_verify(bool existing_attributes[]);

/** @} */
