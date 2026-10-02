/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "attribute.h"
#include "timeline_resources.h"

/**
 * @defgroup services_timeline_metricgroup Metric groups
 * @ingroup services_timeline
 * @brief Builder for the metric names, values and icons attributes of a pin.
 *
 * The lists map to AttributeIdMetricNames, AttributeIdMetricValues and AttributeIdMetricIcons.
 * @{
 */

/** @brief Metric names, values and icons being built. */
typedef struct MetricGroup {
  /** Metric names. */
  struct pbl_string_list *names;
  /** Metric values. */
  struct pbl_string_list *values;
  /** Metric icons, as TimelineResourceId. */
  Uint32List *icons;
  /** Number of metrics added. */
  int num_items;
  /** Capacity, in metrics. */
  int max_num_items;
  /** Maximum size of a name or value string. */
  size_t max_item_string_size;
} MetricGroup;

/**
 * @brief Create a metric group.
 *
 * @param max_num_items Maximum number of metrics.
 * @param max_item_string_size Maximum size of any name or value string.
 * @return New group, allocated on the calling task's heap.
 */
MetricGroup *metric_group_create(int max_num_items, size_t max_item_string_size);

/**
 * @brief Destroy a metric group.
 *
 * @param metric_group Group to destroy, may be NULL.
 */
void metric_group_destroy(MetricGroup *metric_group);

/**
 * @brief Add a metric to a group.
 *
 * @param metric_group Group.
 * @param name_i18n i18n key of the name.
 * @param value Value string.
 * @param icon Icon.
 * @param i18n_owner Owner of the translated name.
 * @return true if added, false if the group is full.
 */
bool metric_group_add_item(MetricGroup *metric_group, const char *name_i18n, const char *value,
                           TimelineResourceId icon, void *i18n_owner);

/** @} */
