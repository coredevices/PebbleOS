/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Shell Preferences
//
// These are preferences which must be available for querying across all shells
// and which must be implemented differently depending on the shell compiled in.
// Only preferences which are used by common services and kernel code meet these
// criteria.
//
// NEW PREFERENCES DO __NOT__ BELONG HERE WITHOUT A VERY GOOD REASON.

#include <stdbool.h>

#include "applib/graphics/gtypes.h"
#include "process_management/app_install_types.h"
#include "shell/system_theme.h"
#include "util/uuid.h"


// The clock 12h/24h setting is required by services/common/clock.c.
bool shell_prefs_get_clock_24h_style(void);
void shell_prefs_set_clock_24h_style(bool is24h);

// The timezone source setting is required by services/common/clock.c.
// When the source is manual, we don't override our timezone with the phone's timezone info
bool shell_prefs_is_timezone_source_manual(void);
void shell_prefs_set_timezone_source_manual(bool manual);

// The timezone id setting is required by services/common/clock.c.
// The automatic timezone id is what we get from the phone
int16_t shell_prefs_get_automatic_timezone_id(void);
void shell_prefs_set_automatic_timezone_id(int16_t timezone_id);

// Preferences for choosing the units that are displayed in various places in the UI
typedef enum UnitsDistance {
  UnitsDistance_KM,
  UnitsDistance_Miles,
  UnitsDistanceCount
} UnitsDistance;

UnitsDistance shell_prefs_get_units_distance(void);
void shell_prefs_set_units_distance(UnitsDistance newUnit);

// The backlight preferences are required in all shells, but the settings are
// hardcoded when running PRF.

// The backlight behaviour enum value is used by the light service analytics.
// This type has been deprecated for any other use, replaced by the enabled
// and ambient_sensor_enabled booleans.
typedef enum BacklightBehaviour {
  BacklightBehaviour_On = 0,
  BacklightBehaviour_Off = 1,
  BacklightBehaviour_Auto = 2,
  NumBacklightBehaviours
} BacklightBehaviour;

bool backlight_is_enabled(void);
void backlight_set_enabled(bool enabled);

bool backlight_is_ambient_sensor_enabled(void);
void backlight_set_ambient_sensor_enabled(bool enabled);

#define DEFAULT_BACKLIGHT_TIMEOUT_MS 3000
uint32_t backlight_get_timeout_ms(void);
void backlight_set_timeout_ms(uint32_t timeout_ms);

uint16_t backlight_get_intensity(void);

uint8_t backlight_get_intensity_percent(void);
void backlight_set_intensity_percent(uint8_t intensity_percent);

// The backlight motion enabled setting is used by the kernel event loop.
bool backlight_is_motion_enabled(void);
void backlight_set_motion_enabled(bool enable);

#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
// Dynamic backlight intensity based on ambient light sensor
bool backlight_is_dynamic_intensity_enabled(void);
void backlight_set_dynamic_intensity_enabled(bool enable);

// Dynamic backlight thresholds (for debug menu)
uint32_t backlight_get_dynamic_min_threshold(void);
void backlight_set_dynamic_min_threshold(uint32_t threshold);
#endif

// Motion sensitivity for accelerometer shake detection (0-100, lower = less sensitive)
// Only available on platforms with LSM6DSO (Asterix, Obelix)
uint8_t shell_prefs_get_motion_sensitivity(void);
void shell_prefs_set_motion_sensitivity(uint8_t sensitivity);

// The backlight ambient light threshold setting
uint32_t backlight_get_ambient_threshold(void);
void backlight_set_ambient_threshold(uint32_t threshold);

// Stationary mode will put the watch in a low power state. Disabling will
// prevent the watch from turning off any features.
bool shell_prefs_get_stationary_enabled(void);
void shell_prefs_set_stationary_enabled(bool enabled);

// The default worker setting is used by process_management.
AppInstallId worker_preferences_get_default_worker(void);
void worker_preferences_set_default_worker(AppInstallId id);

bool shell_prefs_get_language_english(void);
void shell_prefs_set_language_english(bool english);
void shell_prefs_toggle_language_english(void);

// Manage display offset
void shell_prefs_set_display_offset(GPoint offset);
GPoint shell_prefs_get_display_offset(void);
void shell_prefs_display_offset_init(void);
bool shell_prefs_should_prompt_display_calibration(void);
void shell_prefs_set_should_prompt_display_calibration(bool should_prompt);

uint8_t timeline_prefs_get_settings_opened(void);
void timeline_prefs_set_settings_opened(uint8_t version);
void timeline_peek_prefs_set_enabled(bool enabled);
bool timeline_peek_prefs_get_enabled(void);
void timeline_peek_prefs_set_before_time(uint16_t before_time_m);
uint16_t timeline_peek_prefs_get_before_time(void);

bool shell_prefs_can_coredump_on_request(void);
void shell_prefs_set_coredump_on_request(bool enabled);

#if CAPABILITY_HAS_APP_SCALING
// Legacy app rendering mode - whether to use bezel or scaling for legacy apps
typedef enum LegacyAppRenderMode {
  LegacyAppRenderMode_Bezel = 0,    // Center with black bezel (original behavior)
  LegacyAppRenderMode_Scaling = 1,  // Scale to fill screen
  LegacyAppRenderModeCount
} LegacyAppRenderMode;

LegacyAppRenderMode shell_prefs_get_legacy_app_render_mode(void);
void shell_prefs_set_legacy_app_render_mode(LegacyAppRenderMode mode);
#endif

#if CAPABILITY_HAS_ORIENTATION_MANAGER
bool display_orientation_is_left(void);
void display_orientation_set_left(bool left);
#endif

GColor shell_prefs_get_theme_highlight_color(void);
void shell_prefs_set_theme_highlight_color(GColor color);

bool shell_prefs_get_menu_scroll_wrap_around_enable(void);
void shell_prefs_set_menu_scroll_wrap_around_enable(bool enable);

typedef enum MenuScrollVibeBehavior {
  MenuScrollNoVibe,
  MenuScrollVibeOnWrapAround,
  MenuScrollVibeOnLocked,
  MenuScrollVibeBehaviorsCount,
} MenuScrollVibeBehavior;

MenuScrollVibeBehavior shell_prefs_get_menu_scroll_vibe_behavior(void);
void shell_prefs_set_menu_scroll_vibe_behavior(MenuScrollVibeBehavior behavior);
