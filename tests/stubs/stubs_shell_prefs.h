/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "shell/prefs.h"
#include "pbl/kernel/compiler.h"

static bool s_clock_24h;

bool PBL_WEAK shell_prefs_get_clock_24h_style(void) {
  return s_clock_24h;
}

void PBL_WEAK shell_prefs_set_clock_24h_style(bool is_24h) {
  s_clock_24h = is_24h;
}

static bool s_clock_timezone_manual;

bool PBL_WEAK shell_prefs_is_timezone_source_manual(void) {
  return s_clock_timezone_manual;
}

void PBL_WEAK shell_prefs_set_timezone_source_manual(bool manual) {
  s_clock_timezone_manual = manual;
}

static bool s_clock_time_source_manual;

bool PBL_WEAK shell_prefs_is_time_source_manual(void) {
  return s_clock_time_source_manual;
}

void PBL_WEAK shell_prefs_set_time_source_manual(bool manual) {
  s_clock_time_source_manual = manual;
}

static int16_t s_timezone_id;

int16_t shell_prefs_get_automatic_timezone_id(void) {
  return s_timezone_id;
}

void shell_prefs_set_automatic_timezone_id(int16_t timezone_id) {
  s_timezone_id = timezone_id;
}

UnitsDistance PBL_WEAK shell_prefs_get_units_distance(void) {
  return UnitsDistance_Miles;
}

AppInstallId PBL_WEAK worker_preferences_get_default_worker(void) {
  return 0;
}

PreferredContentSize s_content_size = PreferredContentSizeDefault;

void PBL_WEAK system_theme_set_content_size(PreferredContentSize content_size) {
  s_content_size = content_size;
}

PreferredContentSize PBL_WEAK system_theme_get_content_size(void) {
  return (PreferredContentSize)s_content_size;
}

static bool s_menu_scroll_enable = false;

bool PBL_WEAK shell_prefs_get_menu_scroll_wrap_around_enable(void) {
  return s_menu_scroll_enable;
}

void PBL_WEAK shell_prefs_set_menu_scroll_wrap_around_enable(bool enable) {
  s_menu_scroll_enable = enable;
}

static MenuScrollVibeBehavior s_menu_scroll_vibe_behavior = MenuScrollNoVibe;

MenuScrollVibeBehavior PBL_WEAK shell_prefs_get_menu_scroll_vibe_behavior(void) {
  return s_menu_scroll_vibe_behavior;
}

void PBL_WEAK shell_prefs_set_menu_scroll_vibe_behavior(MenuScrollVibeBehavior behavior) {
  s_menu_scroll_vibe_behavior = behavior;
}