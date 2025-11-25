/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/item.h"

void WEAK timeline_item_destroy(TimelineItem* item) {}

void WEAK timeline_item_free_allocated_buffer(TimelineItem *item) { }

TimelineItem * WEAK timeline_item_create_with_attributes(time_t timestamp, uint16_t duration,
                                                         TimelineItemType type, LayoutId layout,
                                                         AttributeList *attr_list,
                                                         TimelineItemActionGroup *action_group) {
  return NULL;
}

bool WEAK timeline_item_action_is_ancs(const TimelineItemAction *action) {
  return false;
}

bool WEAK timeline_item_action_is_dismiss(const TimelineItemAction *action) {
  return false;
}

TimelineItemAction * WEAK timeline_item_find_dismiss_action(const TimelineItem *item) {
  return NULL;
}

bool WEAK timeline_item_is_ancs_notif(const TimelineItem *item) {
  return false;
}
