/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_i18n.h"
#include "stubs_fonts.h"
#include "stubs_graphics.h"
#include "stubs_text_node.h"

#include "shell/prefs.h"
#include "pbl/services/activity/health_util.h"

static UnitsDistance s_units_distance = UnitsDistance_Miles;

UnitsDistance shell_prefs_get_units_distance(void) {
  return s_units_distance;
}

void test_health_util__initialize(void) {
  s_units_distance = UnitsDistance_Miles;
}

void test_health_util__pace(void) {
  cl_assert_equal_i(health_util_get_pace(29, 4800), 10);   // PBL-36661
  cl_assert_equal_i(health_util_get_pace(10, 800), 20);    // less than a mile
  cl_assert_equal_i(health_util_get_pace(820, 262400), 5); // many miles / long distance
}

void test_health_util__metric_weight_round_trip(void) {
  s_units_distance = UnitsDistance_KM;
  cl_assert_equal_i(health_util_weight_dag_to_tenths(7730), 773);
  cl_assert_equal_i(health_util_weight_tenths_to_dag(773), 7730);

  char value[16];
  health_util_format_weight(value, sizeof(value), 7730);
  cl_assert_equal_s(value, "77.3");
}

void test_health_util__imperial_weight_round_trip(void) {
  s_units_distance = UnitsDistance_Miles;
  const int32_t pounds_tenths = health_util_weight_dag_to_tenths(7730);
  cl_assert_equal_i(pounds_tenths, 1704);
  cl_assert_equal_i(health_util_weight_tenths_to_dag(pounds_tenths), 7729);
}
