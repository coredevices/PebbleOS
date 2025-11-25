/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "shell/prefs.h"
#include "util/attributes.h"

static bool s_clock_24h;

bool WEAK shell_prefs_get_clock_24h_style(void) {
  return s_clock_24h;
}

void WEAK shell_prefs_set_clock_24h_style(bool is_24h) {
  s_clock_24h= is_24h;
}

static bool s_clock_timeezone_manual;

bool WEAK shell_prefs_is_timezone_source_manual(void) {
  return s_clock_timeezone_manual;
}

void WEAK shell_prefs_set_timezone_source_manual(bool manual) {
  s_clock_timeezone_manual = manual;
}

static int16_t s_timezone_id;

int16_t shell_prefs_get_automatic_timezone_id(void) {
  return s_timezone_id;
}

void shell_prefs_set_automatic_timezone_id(int16_t timezone_id) {
  s_timezone_id = timezone_id;
}

UnitsDistance WEAK shell_prefs_get_units_distance(void) {
  return UnitsDistance_Miles;
}

AppInstallId WEAK worker_preferences_get_default_worker(void) {
  return 0;
}

PreferredContentSize s_content_size = PreferredContentSizeDefault;

void WEAK system_theme_set_content_size(PreferredContentSize content_size) {
  s_content_size = content_size;
}

PreferredContentSize WEAK system_theme_get_content_size(void) {
  return (PreferredContentSize)s_content_size;
}
