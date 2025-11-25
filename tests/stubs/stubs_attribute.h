/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/attribute.h"

const char *attribute_get_string(const AttributeList *attr_list, AttributeId id,
                                 char *default_value) {
  return NULL;
}

uint8_t attribute_get_uint8(const AttributeList *attr_list, AttributeId id,
                            uint8_t default_value) {
  return 0;
}

uint32_t attribute_get_uint32(const AttributeList *attr_list, AttributeId id,
                              uint32_t default_value) {
  return 0;
}

void attribute_list_add_cstring(AttributeList *list, AttributeId id, const char *cstring) {
  return;
}

void attribute_list_add_uint32(AttributeList *list, AttributeId id, uint32_t uint32) {
  return;
}

void attribute_list_add_string_list(AttributeList *list, AttributeId id, StringList *string_list) {
  return;
}

void attribute_list_add_uint32_list(AttributeList *list, AttributeId id, Uint32List *uint32_list) {
  return;
}

void attribute_list_add_resource_id(AttributeList *list, AttributeId id,
                                    uint32_t resource_id) {
  return;
}

void attribute_list_add_uint8(AttributeList *list, AttributeId id, uint8_t uint8) {
  return;
}

void attribute_list_destroy_list(AttributeList *list) {
  return;
}
