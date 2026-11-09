/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/alarms/alarm.h>
#include <pbl/services/clock.h>
#include <pbl/services/timeline/alarm_layout.h>
#include <pbl/services/timeline/attribute.h>

#include <clar.h>

// Stubs
////////////////////////////////////////////////////////////////

// Functions under test
/////////////////////////

void prv_get_subtitle_from_attributes(AttributeList *attributes, char *buffer, size_t buffer_size,
                                      const void *i18n_owner);

// Setup
/////////////////////////

void test_alarm_layout__initialize(void) {
}

void test_alarm_layout__cleanup(void) {
}

// Tests
///////////////////////////

void test_alarm_layout__get_subtitle_from_attributes(void) {
  char buffer[TIME_STRING_REQUIRED_LENGTH] = {};
  const size_t buffer_size = sizeof(buffer);

  const void *dummy_i18n_owner = (void *)1234;

  AttributeList attribute_list = {};
  AttributeList *attribute_list_ref = &attribute_list;

  // For legacy reasons, an alarm pin that only has a subtitle attribute should use
  // that subtitle, manually making it all-caps using toupper_str() on rectangular displays
  attribute_list_add_cstring(attribute_list_ref, AttributeIdSubtitle, "Weekdays");
  prv_get_subtitle_from_attributes(attribute_list_ref, buffer, buffer_size, dummy_i18n_owner);
  cl_assert_equal_s(buffer, PBL_IF_RECT_ELSE("WEEKDAYS", "Weekdays"));

  // An alarm pin that has both a subtitle attribute and an AlarmKind attribute should create the
  // subtitle using the AlarmKind (ignoring the subtitle attribute), respecting the desire to
  // all-caps the subtitle on rectangular displays
  attribute_list = (AttributeList){};
  attribute_list_add_cstring(attribute_list_ref, AttributeIdSubtitle, "Ignore me!");
  attribute_list_add_uint8(attribute_list_ref, AttributeIdAlarmKind, (uint8_t)ALARM_KIND_JUST_ONCE);
  prv_get_subtitle_from_attributes(attribute_list_ref, buffer, buffer_size, dummy_i18n_owner);
  cl_assert_equal_s(buffer, PBL_IF_RECT_ELSE("ONCE", "Once"));
  attribute_list_destroy_list(attribute_list_ref);
}
