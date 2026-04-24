/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "attribute.h"
#include "timeline_resources.h"

typedef struct MetricGroup {
  StringList *names;
  StringList *values;
  Uint32List *icons;
  int num_items;
  int max_num_items;
  size_t max_item_string_size;
} MetricGroup;

//! Create a metric group
//! @param max_num_items max number of items able to be added to the group
//! @param max_item_string_size max length of any string in the group, name and value
//! @return newly allocated MetricGroup
MetricGroup *metric_group_create(int max_num_items, size_t max_item_string_size);

//! Destroy a metric group
//! @param metric_group MetricGroup to destroy
void metric_group_destroy(MetricGroup *metric_group);

//! Adds an item to a metric group
//! @param metric_group MetricGroup to add an item to
//! @param name_i18n i18n key of the name string
//! @param value value field string
//! @param icon TimelineResourceId icon id
//! @param i18n_owner i18n owner to use
//! @return true if the item was added, false otherwise
bool metric_group_add_item(MetricGroup *metric_group, const char *name_i18n, const char *value,
                           TimelineResourceId icon, void *i18n_owner);
