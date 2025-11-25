/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_i18n.h"
#include "stubs_fonts.h"
#include "stubs_graphics.h"
#include "stubs_text_node.h"

#include "shell/prefs.h"
#include "services/normal/activity/health_util.h"

UnitsDistance shell_prefs_get_units_distance(void) {
  return UnitsDistance_Miles;
}

void test_health_util__pace(void) {
  cl_assert_equal_i(health_util_get_pace(29, 4800), 10); // PBL-36661
  cl_assert_equal_i(health_util_get_pace(10, 800), 20); // less than a mile
  cl_assert_equal_i(health_util_get_pace(820, 262400), 5); // many miles / long distance
}
