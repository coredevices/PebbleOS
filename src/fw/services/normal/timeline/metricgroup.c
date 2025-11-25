/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "metricgroup.h"

#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"
#include "services/normal/timeline/attribute.h"

MetricGroup *metric_group_create(int max_num_items, size_t max_item_string_size) {
  const size_t max_list_size = StringListSize(max_num_items, max_item_string_size);
  MetricGroup *metric = task_zalloc_check(sizeof(MetricGroup));
  *metric = (MetricGroup) {
    .names = task_zalloc_check(max_list_size),
    .values = task_zalloc_check(max_list_size),
    .icons = task_zalloc_check(Uint32ListSize(max_num_items)),
    .max_num_items = max_num_items,
    .max_item_string_size = max_item_string_size,
  };
  return metric;
}

void metric_group_destroy(MetricGroup *metric_group) {
  if (!metric_group) {
    return;
  }
  task_free(metric_group->names);
  task_free(metric_group->values);
  task_free(metric_group->icons);
  task_free(metric_group);
}

bool metric_group_add_item(MetricGroup *metric_group, const char *name_i18n, const char *value,
                           TimelineResourceId icon, void *i18n_owner) {
  if (metric_group->num_items >= metric_group->max_num_items) {
    return false;
  }
  const size_t max_list_size = StringListSize(metric_group->max_num_items,
                                              metric_group->max_item_string_size);
  string_list_add_string(metric_group->names, max_list_size, i18n_get(name_i18n, i18n_owner),
                         metric_group->max_item_string_size);
  string_list_add_string(metric_group->values, max_list_size, value,
                         metric_group->max_item_string_size);
  metric_group->icons->values[metric_group->num_items++] = icon;
  metric_group->icons->num_values = metric_group->num_items;
  return true;
}
