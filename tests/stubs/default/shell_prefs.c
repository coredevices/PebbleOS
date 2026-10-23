/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <shell/prefs.h>

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

PBL_WEAK int16_t shell_prefs_get_automatic_timezone_id(void) {
  return s_timezone_id;
}

PBL_WEAK void shell_prefs_set_automatic_timezone_id(int16_t timezone_id) {
  s_timezone_id = timezone_id;
}

UnitsDistance PBL_WEAK shell_prefs_get_units_distance(void) {
  return UnitsDistance_Miles;
}

AppInstallId PBL_WEAK worker_preferences_get_default_worker(void) {
  return 0;
}

PBL_WEAK PreferredContentSize s_content_size = PreferredContentSizeDefault;

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

static GColor s_theme_highlight_color = {.argb = GColorVividCeruleanARGB8};
static GColor s_theme_secondary_color = {.argb = GColorCobaltBlueARGB8};
static GColor s_theme_background_color = {.argb = GColorClearARGB8};
static GColor s_theme_text_color = {.argb = GColorClearARGB8};
static Uuid s_theme_active_uuid = UUID_INVALID_INIT;
static uint32_t s_theme_bg_resource_id = 0;
static ThemeAppColorMode s_theme_app_color_mode = ThemeAppColorMode_ColorsAndBackground;

GColor PBL_WEAK shell_prefs_get_theme_highlight_color(void) {
  return PBL_IF_COLOR_ELSE(s_theme_highlight_color, GColorBlack);
}

void PBL_WEAK shell_prefs_set_theme_highlight_color(GColor color) {
  s_theme_highlight_color = color;
}

GColor PBL_WEAK shell_prefs_get_theme_secondary_color(void) {
  return PBL_IF_COLOR_ELSE(s_theme_secondary_color, GColorBlack);
}

void PBL_WEAK shell_prefs_set_theme_secondary_color(GColor color) {
  s_theme_secondary_color = color;
}

GColor PBL_WEAK shell_prefs_get_theme_background_color(void) {
  return (s_theme_background_color.argb == GColorClear.argb) ? GColorWhite
                                                             : s_theme_background_color;
}

void PBL_WEAK shell_prefs_set_theme_background_color(GColor color) {
  s_theme_background_color = color;
}

bool PBL_WEAK shell_prefs_is_theme_background_default(void) {
  return (s_theme_background_color.argb == GColorClear.argb) && (s_theme_bg_resource_id == 0);
}

GColor PBL_WEAK shell_prefs_get_theme_text_color(void) {
  return s_theme_text_color;
}

void PBL_WEAK shell_prefs_set_theme_text_color(GColor color) {
  s_theme_text_color = color;
}

Uuid PBL_WEAK shell_prefs_get_theme_active_uuid(void) {
  return s_theme_active_uuid;
}

void PBL_WEAK shell_prefs_set_theme_active_uuid(const Uuid *uuid) {
  s_theme_active_uuid = uuid ? *uuid : UUID_INVALID;
}

uint32_t PBL_WEAK shell_prefs_get_theme_bg_resource_id(void) {
  return s_theme_bg_resource_id;
}

void PBL_WEAK shell_prefs_set_theme_bg_resource_id(uint32_t resource_id) {
  s_theme_bg_resource_id = resource_id;
}

ThemeAppColorMode PBL_WEAK shell_prefs_get_theme_app_color_mode(void) {
  return s_theme_app_color_mode;
}

void PBL_WEAK shell_prefs_set_theme_app_color_mode(ThemeAppColorMode mode) {
  s_theme_app_color_mode = mode;
}