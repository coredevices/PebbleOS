/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_smartstrap.h"

SmartstrapResult app_smartstrap_subscribe(SmartstrapHandlers handlers) {
  return SmartstrapResultNotPresent;
}

void app_smartstrap_unsubscribe(void) {
}

void app_smartstrap_set_timeout(uint16_t timeout_ms) {
}

SmartstrapAttribute *app_smartstrap_attribute_create(SmartstrapServiceId service_id,
                                                     SmartstrapAttributeId attribute_id,
                                                     size_t buffer_length) {
  return NULL;
}

void app_smartstrap_attribute_destroy(SmartstrapAttribute *attribute) {
}

bool app_smartstrap_service_is_available(SmartstrapServiceId service_id) {
  return false;
}

SmartstrapServiceId app_smartstrap_attribute_get_service_id(SmartstrapAttribute *attribute) {
  return 0;
}

SmartstrapAttributeId app_smartstrap_attribute_get_attribute_id(SmartstrapAttribute *attribute) {
  return 0;
}

SmartstrapResult app_smartstrap_attribute_read(SmartstrapAttribute *attribute) {
  return SmartstrapResultNotPresent;
}

SmartstrapResult app_smartstrap_attribute_begin_write(SmartstrapAttribute *attribute,
                                                      uint8_t **buffer, size_t *buffer_length) {
  return SmartstrapResultNotPresent;
}

SmartstrapResult app_smartstrap_attribute_end_write(SmartstrapAttribute *attribute,
                                                    size_t write_length, bool request_read) {
  return SmartstrapResultNotPresent;
}
