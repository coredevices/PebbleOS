/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/util/generic_attr.h"

#include <string.h>

struct pbl_generic_attr *pbl_generic_attr_find(struct pbl_generic_attr_list *attr_list, uint8_t id,
                                               size_t size) {
  uint8_t *cursor = (uint8_t *)(attr_list->attributes);
  uint8_t *end = (uint8_t *)attr_list + size;
  for (unsigned int i = 0; i < attr_list->num_attributes; i++) {
    struct pbl_generic_attr *attribute = (struct pbl_generic_attr *)cursor;

    // Check that we do not read past the end of the buffer
    if ((cursor + sizeof(struct pbl_generic_attr) > end) ||
        (attribute->data + attribute->length > end)) {
      return NULL;
    }

    if (attribute->id == id) {
      return attribute;
    }
    cursor += sizeof(struct pbl_generic_attr) + attribute->length;
  }
  return NULL;
}

struct pbl_generic_attr *pbl_generic_attr_add(struct pbl_generic_attr *attr, uint8_t id,
                                              const void *data, size_t size) {
  *attr = (struct pbl_generic_attr){
    .id = id,
    .length = size,
  };
  memcpy(attr->data, data, size);

  uint8_t *cursor = (uint8_t *)attr;
  cursor += sizeof(struct pbl_generic_attr) + size;
  return (struct pbl_generic_attr *)cursor;
}
