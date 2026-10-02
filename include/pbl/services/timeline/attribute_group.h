/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/item.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup services_timeline_attribute_group Attribute groups
 * @ingroup services_timeline
 * @brief Shared code for data made of an attribute list and a group of elements with their own
 * attributes.
 *
 * The elements ("group data") are either the actions of a TimelineItemActionGroup or the addresses
 * of an AddressList. Use the wrappers in @ref services_timeline_attributes_actions or
 * @c pbl/services/contacts/attributes_address.h instead, which document the parameters.
 *
 * Serialized, the attributes are followed by each element: an element header (action id, type and
 * attribute count, or address UUID, type and attribute count) and its attributes. In memory, one
 * buffer holds the attributes, the elements, each element's attributes and finally the string
 * data.
 * @{
 */

/** @brief Kind of group elements. */
typedef enum {
  /** Elements are TimelineItemAction. */
  AttributeGroupType_Action,
  /** Elements are Address. */
  AttributeGroupType_Address,
} AttributeGroupType;

/**
 * @brief Measure serialized group data.
 *
 * @param type Kind of group elements.
 * @param num_attributes Number of top-level attributes.
 * @param num_group_type_elements Number of elements.
 * @param data Serialized data.
 * @param size Size of @p data in bytes.
 * @param[out] string_alloc_size_out Size of the string data.
 * @param[out] attributes_per_group_type_element_out Attribute count of each element.
 * @return true if the data is well formed.
 */
bool attribute_group_parse_serial_data(AttributeGroupType type, uint8_t num_attributes,
                                       uint8_t num_group_type_elements, const uint8_t *data,
                                       size_t size, size_t *string_alloc_size_out,
                                       uint8_t *attributes_per_group_type_element_out);

/**
 * @brief Get the size of the buffer holding group data.
 *
 * @param type Kind of group elements.
 * @param num_attributes Number of top-level attributes.
 * @param num_group_type_elements Number of elements.
 * @param attributes_per_group_type_element Attribute count of each element.
 * @param required_size_for_strings Size of the string data.
 * @return Size in bytes.
 */
size_t attribute_group_get_required_buffer_size(AttributeGroupType type, uint8_t num_attributes,
                                                uint8_t num_group_type_elements,
                                                const uint8_t *attributes_per_group_type_element,
                                                size_t required_size_for_strings);

/**
 * @brief Lay out an attribute list and group in a buffer.
 *
 * @param type Kind of group elements.
 * @param[out] attr_list Top-level attribute list.
 * @param[out] group_ptr TimelineItemActionGroup or AddressList.
 * @param[in,out] buffer Buffer; advanced to the start of the string data.
 * @param num_attributes Number of top-level attributes.
 * @param num_group_type_elements Number of elements.
 * @param attributes_per_group_type_element Attribute count of each element.
 */
void attribute_group_init(AttributeGroupType type, AttributeList *attr_list, void *group_ptr,
                          uint8_t **buffer, uint8_t num_attributes, uint8_t num_group_type_elements,
                          const uint8_t *attributes_per_group_type_element);

/**
 * @brief Deserialize group data into a list and group set up with attribute_group_init().
 *
 * @param type Kind of group elements.
 * @param[in,out] attr_list Top-level attribute list.
 * @param[in,out] group_ptr TimelineItemActionGroup or AddressList.
 * @param buffer String data buffer.
 * @param buf_end End of the string data buffer.
 * @param payload Serialized data.
 * @param payload_size Size of @p payload in bytes.
 * @return true on success.
 */
bool attribute_group_deserialize(AttributeGroupType type, AttributeList *attr_list, void *group_ptr,
                                 uint8_t *buffer, uint8_t *buf_end, const uint8_t *payload,
                                 size_t payload_size);

/**
 * @brief Get the serialized size of group data.
 *
 * @param type Kind of group elements.
 * @param attr_list Top-level attribute list, may be NULL.
 * @param group_ptr TimelineItemActionGroup or AddressList, may be NULL.
 * @return Size in bytes.
 */
size_t attribute_group_get_serialized_payload_size(AttributeGroupType type,
                                                   AttributeList *attr_list, void *group_ptr);

/**
 * @brief Serialize group data.
 *
 * @param type Kind of group elements.
 * @param attr_list Top-level attribute list, may be NULL.
 * @param group_ptr TimelineItemActionGroup or AddressList, may be NULL.
 * @param[out] buffer Output buffer.
 * @param buffer_size Size of @p buffer in bytes.
 * @return Number of bytes written.
 */
size_t attribute_group_serialize_payload(AttributeGroupType type, AttributeList *attr_list,
                                         void *group_ptr, uint8_t *buffer, size_t buffer_size);

/** @} */
