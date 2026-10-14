/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/graphics/graphics.h>
#include <apps/system/timeline/text_node.h>

PBL_WEAK int health_util_format_hours_and_minutes(char *buffer, size_t buffer_size, int duration_s,
                                                  void *i18n_owner) {
  return 0;
}

PBL_WEAK int health_util_format_minutes_and_seconds(char *buffer, size_t buffer_size,
                                                    int duration_s, void *i18n_owner) {
  return 0;
}

PBL_WEAK int health_util_format_hours_minutes_seconds(char *buffer, size_t buffer_size,
                                                      int duration_s, bool leading_zero,
                                                      void *i18n_owner) {
  return 0;
}

PBL_WEAK void health_util_duration_to_hours_and_minutes_text_node(int duration_s, void *i18n_owner,
                                                                  GFont number_font,
                                                                  GFont units_font,
                                                                  GTextNodeContainer *container) {
}

PBL_WEAK void health_util_convert_fraction_to_whole_and_decimal_part(int numerator, int denominator,
                                                                     int *whole_part,
                                                                     int *decimal_part) {
}

PBL_WEAK int health_util_format_whole_and_decimal(char *buffer, size_t buffer_size, int numerator,
                                                  int denominator) {
  return 0;
}

PBL_WEAK int health_util_get_distance_factor(void) {
  return 1;
}

PBL_WEAK const char *health_util_get_distance_string(const char *miles_string,
                                                     const char *km_string) {
  return miles_string;
}

PBL_WEAK int health_util_format_distance(char *buffer, size_t buffer_size, uint32_t distance_m) {
  return 0;
}

PBL_WEAK void health_util_convert_distance_to_whole_and_decimal_part(int distance_m,
                                                                     int *whole_part,
                                                                     int *decimal_part) {
}

PBL_WEAK int health_util_get_pace(int time_min, int distance_meter) {
  return 0;
}
