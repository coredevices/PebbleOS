/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "attributes_address.h"

#include "services/normal/timeline/attribute_group.h"


#define GROUP_TYPE AttributeGroupType_Address

bool attributes_address_parse_serial_data(uint8_t num_attributes,
                                          uint8_t num_addresses,
                                          const uint8_t *data,
                                          size_t size,
                                          size_t *string_alloc_size_out,
                                          uint8_t *attributes_per_address_out) {

  return attribute_group_parse_serial_data(GROUP_TYPE,
                                           num_attributes,
                                           num_addresses,
                                           data,
                                           size,
                                           string_alloc_size_out,
                                           attributes_per_address_out);
}

size_t attributes_address_get_buffer_size(uint8_t num_attributes,
                                          uint8_t num_addresses,
                                          const uint8_t *attributes_per_address,
                                          size_t required_size_for_strings) {

  return attribute_group_get_required_buffer_size(GROUP_TYPE,
                                                  num_attributes,
                                                  num_addresses,
                                                  attributes_per_address,
                                                  required_size_for_strings);
}

void attributes_address_init(AttributeList *attr_list,
                             AddressList *addr_list,
                             uint8_t **buffer,
                             uint8_t num_attributes,
                             uint8_t num_addresses,
                             const uint8_t *attributes_per_address) {

  attribute_group_init(GROUP_TYPE,
                       attr_list,
                       addr_list,
                       buffer,
                       num_attributes,
                       num_addresses,
                       attributes_per_address);
}

bool attributes_address_deserialize(AttributeList *attr_list,
                                    AddressList *addr_list,
                                    uint8_t *buffer,
                                    uint8_t *buf_end,
                                    const uint8_t *payload,
                                    size_t payload_size) {

  return attribute_group_deserialize(GROUP_TYPE,
                                     attr_list,
                                     addr_list,
                                     buffer,
                                     buf_end,
                                     payload,
                                     payload_size);
}

size_t attributes_address_get_serialized_payload_size(AttributeList *attr_list,
                                                      AddressList *addr_list) {
  return attribute_group_get_serialized_payload_size(GROUP_TYPE,
                                                     attr_list,
                                                     addr_list);
}

size_t attributes_address_serialize_payload(AttributeList *attr_list,
                                            AddressList *addr_list,
                                            uint8_t *buffer,
                                            size_t buffer_size) {
  return attribute_group_serialize_payload(GROUP_TYPE,
                                           attr_list,
                                           addr_list,
                                           buffer,
                                           buffer_size);
}
