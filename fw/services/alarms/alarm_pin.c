/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/alarms/alarm_pin.h"

#include "kernel/pbl_malloc.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/blob_db/pin_db.h"
#include "pbl/services/timeline/attribute.h"
#include "pbl/services/timeline/timeline.h"
#include "pbl/services/timeline/timeline_resources.h"

#define ALARM_PIN_REMOVE_BATCH_SIZE 8

typedef struct {
  time_t now;
  const Uuid *tracked;
  size_t tracked_count;
  Uuid ids[ALARM_PIN_REMOVE_BATCH_SIZE];
  size_t count;
} UntrackedAlarmPins;

static bool prv_collect_untracked_future_pin(SettingsFile *file, SettingsRecordInfo *info,
                                             void *context) {
  UntrackedAlarmPins *pins = context;
  if (info->key_len != UUID_SIZE || info->val_len < (int)sizeof(SerializedTimelineItemHeader)) {
    return true;
  }

  SerializedTimelineItemHeader header;
  info->get_val(file, &header, sizeof(header));
  const Uuid alarm_source = UUID_ALARMS_DATA_SOURCE;
  if (header.common.type != TimelineItemTypePin ||
      !uuid_equal(&header.common.parent_id, &alarm_source) ||
      header.common.layout != LayoutIdAlarm || header.common.timestamp < pins->now) {
    return true;
  }

  Uuid id;
  info->get_key(file, &id, sizeof(id));
  for (size_t i = 0; i < pins->tracked_count; ++i) {
    if (uuid_equal(&id, &pins->tracked[i])) {
      return true;
    }
  }

  pins->ids[pins->count++] = id;
  return pins->count < ALARM_PIN_REMOVE_BATCH_SIZE;
}

status_t alarm_pin_remove_untracked_future(time_t now, const Uuid *tracked, size_t tracked_count) {
  UntrackedAlarmPins pins = {.now = now, .tracked = tracked, .tracked_count = tracked_count};
  do {
    pins.count = 0;
    status_t rv = pin_db_each(prv_collect_untracked_future_pin, &pins);
    if (rv != S_SUCCESS) {
      return rv;
    }
    for (size_t i = 0; i < pins.count; ++i) {
      rv = pin_db_delete((const uint8_t *)&pins.ids[i], sizeof(Uuid));
      if (rv != S_SUCCESS) {
        return rv;
      }
    }
  } while (pins.count == ALARM_PIN_REMOVE_BATCH_SIZE);

  return S_SUCCESS;
}

// ----------------------------------------------------------------------------------------------
//! Sets attributes for an alarm pin
static void prv_set_pin_attributes(AttributeList *list, AlarmType type, AlarmKind kind) {
  const bool is_smart = (type == AlarmType_Smart);
  attribute_list_add_cstring(list, AttributeIdTitle,
                             is_smart ? i18n_get("Smart Alarm", list) : i18n_get("Alarm", list));
  attribute_list_add_resource_id(
      list, AttributeIdIconPin,
      is_smart ? TIMELINE_RESOURCE_SMART_ALARM : TIMELINE_RESOURCE_ALARM_CLOCK);
  attribute_list_add_resource_id(list, AttributeIdIconTiny, TIMELINE_RESOURCE_ALARM_CLOCK);
  const bool all_caps = false;
  const char *alarm_string = i18n_get(alarm_get_string_for_kind(kind, all_caps), list);
  attribute_list_add_cstring(list, AttributeIdSubtitle, alarm_string);
  attribute_list_add_uint8(list, AttributeIdAlarmKind, kind);
}

// ----------------------------------------------------------------------------------------------
static void prv_set_edit_action_attributes(AttributeList *list, AlarmId id) {
  attribute_list_add_cstring(list, AttributeIdTitle, i18n_get("Edit", list));
  attribute_list_add_uint32(list, AttributeIdLaunchCode, (uint32_t)id);
}

// ----------------------------------------------------------------------------------------------
status_t alarm_pin_add(time_t alarm_time, AlarmId id, AlarmType type, AlarmKind kind,
                       Uuid *uuid_out) {
  const unsigned num_actions = 1; // We are just supporting "edit" for now
  TimelineItemActionGroup action_group = {
    .num_actions = num_actions,
    .actions = task_zalloc_check(sizeof(TimelineItemAction) * num_actions),
  };

  AttributeList edit_attr_list = {0};
  prv_set_edit_action_attributes(&edit_attr_list, id);
  action_group.actions[0] = (TimelineItemAction){
    .id = (uint8_t)id, // id is guaranteed to be valid here, and we only support 10 alarms
    .type = TimelineItemActionTypeOpenWatchApp,
    .attr_list = edit_attr_list,
  };

  AttributeList pin_attr_list = {0};
  prv_set_pin_attributes(&pin_attr_list, type, kind);
  TimelineItem *item = timeline_item_create_with_attributes(
      alarm_time, 0, TimelineItemTypePin, LayoutIdAlarm, &pin_attr_list, &action_group);
  if (!item) {
    i18n_free_all(&pin_attr_list);
    i18n_free_all(&edit_attr_list);
    attribute_list_destroy_list(&pin_attr_list);
    attribute_list_destroy_list(&edit_attr_list);
    task_free(action_group.actions);
    return E_OUT_OF_MEMORY;
  }
  item->header.from_watch = true;
  item->header.parent_id = (Uuid)UUID_ALARMS_DATA_SOURCE;

  status_t rv = pin_db_insert_item_without_event(item);

  i18n_free_all(&pin_attr_list);
  i18n_free_all(&edit_attr_list);
  attribute_list_destroy_list(&pin_attr_list);
  attribute_list_destroy_list(&edit_attr_list);
  task_free(action_group.actions);

  if (rv == S_SUCCESS && uuid_out) {
    *uuid_out = item->header.id;
  }

  timeline_item_destroy(item);
  return rv;
}

// ----------------------------------------------------------------------------------------------
void alarm_pin_remove(Uuid *alarm_id) {
  pin_db_delete((uint8_t *)alarm_id, sizeof(Uuid));
}
