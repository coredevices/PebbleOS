/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "attributes_actions.h"

#include "services/normal/timeline/attribute_group.h"

#include "system/logging.h"


#define GROUP_TYPE AttributeGroupType_Action

bool attributes_actions_parse_serial_data(uint8_t num_attributes,
                                          uint8_t num_actions,
                                          const uint8_t *data,
                                          size_t size,
                                          size_t *string_alloc_size_out,
                                          uint8_t *attributes_per_actions_out) {

  return attribute_group_parse_serial_data(GROUP_TYPE,
                                           num_attributes,
                                           num_actions,
                                           data,
                                           size,
                                           string_alloc_size_out,
                                           attributes_per_actions_out);
}

size_t attributes_actions_get_required_buffer_size(uint8_t num_attributes,
                                                   uint8_t num_actions,
                                                   uint8_t *attributes_per_actions,
                                                   size_t required_size_for_strings) {

  return attribute_group_get_required_buffer_size(GROUP_TYPE,
                                                  num_attributes,
                                                  num_actions,
                                                  attributes_per_actions,
                                                  required_size_for_strings);
}

void attributes_actions_init(AttributeList *attr_list,
                             TimelineItemActionGroup *action_group,
                             uint8_t **buffer,
                             uint8_t num_attributes,
                             uint8_t num_actions,
                             const uint8_t *attributes_per_actions) {

  attribute_group_init(GROUP_TYPE,
                       attr_list,
                       action_group,
                       buffer,
                       num_attributes,
                       num_actions,
                       attributes_per_actions);
}

bool attributes_actions_deserialize(AttributeList *attr_list,
                                    TimelineItemActionGroup *action_group,
                                    uint8_t *buffer,
                                    uint8_t *buf_end,
                                    const uint8_t *payload,
                                    size_t payload_size) {

  return attribute_group_deserialize(GROUP_TYPE,
                                     attr_list,
                                     action_group,
                                     buffer,
                                     buf_end,
                                     payload,
                                     payload_size);
}

size_t attributes_actions_get_serialized_payload_size(AttributeList *attr_list,
                                                      TimelineItemActionGroup *action_group) {
  return attribute_group_get_serialized_payload_size(GROUP_TYPE,
                                                     attr_list,
                                                     action_group);
}

size_t attributes_actions_serialize_payload(AttributeList *attr_list,
                                            TimelineItemActionGroup *action_group,
                                            uint8_t *buffer,
                                            size_t buffer_size) {
  return attribute_group_serialize_payload(GROUP_TYPE,
                                           attr_list,
                                           action_group,
                                           buffer,
                                           buffer_size);
}

size_t attributes_actions_get_buffer_size(AttributeList *attr_list,
                                          TimelineItemActionGroup *action_group) {
  size_t data_size = 0;
  if (attr_list) {
    data_size += attribute_list_get_buffer_size(attr_list);
  }
  if (action_group) {
    data_size += sizeof(TimelineItemAction) * action_group->num_actions;
    for (int i = 0; i < action_group->num_actions; i++) {
      data_size += attribute_list_get_buffer_size(&action_group->actions[i].attr_list);
    }
  }
  return data_size;
}

static bool prv_action_group_copy(TimelineItemActionGroup *dest, TimelineItemActionGroup *source,
                                  uint8_t *buffer, uint8_t *const buffer_end) {
  const size_t actions_size = sizeof(TimelineItemAction) * source->num_actions;
  if (buffer + actions_size > buffer_end) {
    return false;
  }

  dest->num_actions = source->num_actions;
  dest->actions = (TimelineItemAction*) buffer;
  memcpy(dest->actions, source->actions, actions_size);

  size_t offset = actions_size;
  for (int i = 0; i < source->num_actions; i++) {
    size_t attr_size = attribute_list_get_buffer_size(&source->actions[i].attr_list);
    if (buffer + offset + attr_size > buffer_end) {
      return false;
    }

    attribute_list_copy(&dest->actions[i].attr_list, &source->actions[i].attr_list,
                        buffer + offset, buffer + offset + attr_size);
    offset += attr_size;
  }

  return true;
}

bool attributes_actions_deep_copy(AttributeList *src_attr_list,
                                  AttributeList *dest_attr_list,
                                  TimelineItemActionGroup *src_action_group,
                                  TimelineItemActionGroup *dest_action_group,
                                  uint8_t *buffer, uint8_t *buf_end) {
  bool rv;
  const size_t attr_list_size = attribute_list_get_buffer_size(src_attr_list);

  if (src_attr_list && dest_attr_list) {
    rv = attribute_list_copy(dest_attr_list, src_attr_list,
                             buffer, MIN(buffer + attr_list_size, buf_end));
    if (!rv) {
      PBL_LOG(LOG_LEVEL_ERROR, "Error deep-copying pin attribute list");
      return false;
    }
  }
  if (src_action_group && dest_action_group) {
    rv = prv_action_group_copy(dest_action_group, src_action_group,
                               buffer + attr_list_size, buf_end);
    if (!rv) {
      PBL_LOG(LOG_LEVEL_ERROR, "Error deep-copying pin action group");
      return false;
    }
  }
  return true;
}
