/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "generic_attribute.h"

#include "system/logging.h"

GenericAttribute *generic_attribute_find_attribute(GenericAttributeList *attr_list, uint8_t id,
                                                   size_t size) {
  uint8_t *cursor = (uint8_t *)(attr_list->attributes);
  uint8_t *end = (uint8_t *)attr_list + size;
  for (unsigned int i = 0; i < attr_list->num_attributes; i++) {
    GenericAttribute *attribute = (GenericAttribute *)cursor;

    // Check that we do not read past the end of the buffer
    if ((cursor + sizeof(GenericAttribute) >= end) || (attribute->data + attribute->length > end)) {
      PBL_LOG(LOG_LEVEL_WARNING, "Attribute list is invalid");
      return NULL;
    }

    if (attribute->id == id) {
      return attribute;
    }
    cursor += sizeof(GenericAttribute) + attribute->length;
  }
  return NULL;
}

GenericAttribute *generic_attribute_add_attribute(GenericAttribute *attr, uint8_t id, void *data,
                                                  size_t size) {
  *attr = (GenericAttribute) {
    .id = id,
    .length = size,
  };
  memcpy(attr->data, data, size);

  uint8_t *cursor = (uint8_t *)attr;
  cursor += sizeof(GenericAttribute) + size;
  return (GenericAttribute *)cursor;
}
