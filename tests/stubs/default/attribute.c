/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/timeline/attribute.h>

PBL_WEAK const char *attribute_get_string(const AttributeList *attr_list, AttributeId id,
                                          char *default_value) {
  return nullptr;
}

PBL_WEAK uint8_t attribute_get_uint8(const AttributeList *attr_list, AttributeId id,
                                     uint8_t default_value) {
  return 0;
}

PBL_WEAK uint32_t attribute_get_uint32(const AttributeList *attr_list, AttributeId id,
                                       uint32_t default_value) {
  return 0;
}

PBL_WEAK void attribute_list_add_cstring(AttributeList *list, AttributeId id, const char *cstring) {
  return;
}

PBL_WEAK void attribute_list_add_uint32(AttributeList *list, AttributeId id, uint32_t uint32) {
  return;
}

PBL_WEAK void attribute_list_add_string_list(AttributeList *list, AttributeId id,
                                             struct pbl_string_list *string_list) {
  return;
}

PBL_WEAK void attribute_list_add_uint32_list(AttributeList *list, AttributeId id,
                                             Uint32List *uint32_list) {
  return;
}

PBL_WEAK void attribute_list_add_resource_id(AttributeList *list, AttributeId id,
                                             uint32_t resource_id) {
  return;
}

PBL_WEAK void attribute_list_add_uint8(AttributeList *list, AttributeId id, uint8_t uint8) {
  return;
}

PBL_WEAK void attribute_list_destroy_list(AttributeList *list) {
  return;
}
