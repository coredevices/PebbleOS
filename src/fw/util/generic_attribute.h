/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

#include <inttypes.h>
#include <stddef.h>

typedef struct PACKED GenericAttribute {
  uint8_t id;
  uint16_t length;
  uint8_t data[];
} GenericAttribute;

typedef struct PACKED GenericAttributeList {
  uint8_t num_attributes;
  GenericAttribute attributes[];
} GenericAttributeList;

GenericAttribute *generic_attribute_find_attribute(GenericAttributeList *attr_list, uint8_t id,
                                                   size_t size);

GenericAttribute *generic_attribute_add_attribute(GenericAttribute *attr, uint8_t id, void *data,
                                                  size_t size);
