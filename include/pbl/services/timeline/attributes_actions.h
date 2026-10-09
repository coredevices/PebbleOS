/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "item.h"

#include <stdint.h>

/**
 * @defgroup services_timeline_attributes_actions Attributes and actions
 * @ingroup services_timeline
 * @brief Storage and serialization of an attribute list with an action group.
 *
 * Deserializing is done in three steps: attributes_actions_parse_serial_data() measures the data,
 * attributes_actions_init() lays out a buffer of attributes_actions_get_required_buffer_size()
 * bytes, and attributes_actions_deserialize() fills it.
 *
 * The serialized payload is the attributes followed by each action: id, type and attribute count
 * bytes, then the action's attributes.
 * @{
 */

/**
 * @brief Measure a serialized payload.
 *
 * @param num_attributes Number of non-action attributes.
 * @param num_actions Number of actions.
 * @param data Serialized payload.
 * @param data_size Size of @p data in bytes.
 * @param[out] string_alloc_size_out Size of the string buffer required.
 * @param[out] attributes_per_action_out Attribute count of each action, in action order.
 * @return true if the data was parsed successfully.
 */
bool attributes_actions_parse_serial_data(uint8_t num_attributes, uint8_t num_actions,
                                          const uint8_t *data, size_t data_size,
                                          size_t *string_alloc_size_out,
                                          uint8_t *attributes_per_action_out);

/**
 * @brief Get the size of the buffer holding attributes, actions and their strings.
 *
 * @param num_attributes Number of non-action attributes.
 * @param num_actions Number of actions.
 * @param attributes_per_action Attribute count of each action, in action order.
 * @param required_size_for_strings Total size of the attribute strings.
 * @return Size in bytes.
 */
size_t attributes_actions_get_required_buffer_size(uint8_t num_attributes, uint8_t num_actions,
                                                   uint8_t *attributes_per_action,
                                                   size_t required_size_for_strings);

/**
 * @brief Get the size of the buffer holding a deep copy of an attribute list and action group.
 *
 * @param attr_list Attribute list, may be NULL.
 * @param action_group Action group, may be NULL.
 * @return Size in bytes.
 */
size_t attributes_actions_get_buffer_size(AttributeList *attr_list,
                                          TimelineItemActionGroup *action_group);

/**
 * @brief Lay out an attribute list and action group in a buffer.
 *
 * @param[out] attr_list Attribute list to initialize.
 * @param[out] action_group Action group to initialize.
 * @param[in,out] buffer Buffer for the attributes and actions; advanced to the start of the string
 *                       space.
 * @param num_attributes Number of attributes.
 * @param num_actions Number of actions.
 * @param attributes_per_action Attribute count of each action, in action order.
 */
void attributes_actions_init(AttributeList *attr_list, TimelineItemActionGroup *action_group,
                             uint8_t **buffer, uint8_t num_attributes, uint8_t num_actions,
                             const uint8_t *attributes_per_action);

/**
 * @brief Fill an attribute list and action group from a serialized payload.
 *
 * @param[in,out] attr_list Attribute list set up with attributes_actions_init().
 * @param[in,out] action_group Action group set up with attributes_actions_init().
 * @param buffer String space.
 * @param buf_end End of the string space.
 * @param payload Serialized payload.
 * @param payload_size Size of @p payload in bytes.
 * @return true on success.
 */
bool attributes_actions_deserialize(AttributeList *attr_list, TimelineItemActionGroup *action_group,
                                    uint8_t *buffer, uint8_t *buf_end, const uint8_t *payload,
                                    size_t payload_size);

/**
 * @brief Get the serialized size of an attribute list and action group.
 *
 * @param list Attribute list, may be NULL.
 * @param action_group Action group, may be NULL.
 * @return Size in bytes.
 */
size_t attributes_actions_get_serialized_payload_size(AttributeList *list,
                                                      TimelineItemActionGroup *action_group);

/**
 * @brief Serialize an attribute list and action group.
 *
 * @param attr_list Attribute list, may be NULL.
 * @param action_group Action group, may be NULL.
 * @param[out] buffer Output buffer.
 * @param buffer_size Size of @p buffer in bytes.
 * @return Number of bytes written.
 */
size_t attributes_actions_serialize_payload(AttributeList *attr_list,
                                            TimelineItemActionGroup *action_group, uint8_t *buffer,
                                            size_t buffer_size);

/**
 * @brief Deep copy an attribute list and action group into one buffer.
 *
 * @param src_attr_list Source attribute list.
 * @param[out] dest_attr_list Destination attribute list.
 * @param src_action_group Source action group, may be NULL.
 * @param[out] dest_action_group Destination action group, may be NULL.
 * @param buffer Buffer of at least attributes_actions_get_buffer_size() bytes.
 * @param buf_end End of the buffer.
 * @return true on success, false if the buffer is too small.
 */
bool attributes_actions_deep_copy(AttributeList *src_attr_list, AttributeList *dest_attr_list,
                                  TimelineItemActionGroup *src_action_group,
                                  TimelineItemActionGroup *dest_action_group, uint8_t *buffer,
                                  uint8_t *buf_end);

/** @} */
