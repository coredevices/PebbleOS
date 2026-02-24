/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "quick_launch.h"
#include "shell/normal/quick_launch.h"
#include "shell/normal/watchface.h"
#include "shell/normal/prefs_sync.h"
#include "shell/prefs.h"
#include "shell/prefs_private.h"
#include "shell/system_theme.h"

#include "apps/system/timeline/timeline.h"
#include "apps/system/toggle/quiet_time.h"
#include "board/board.h"
#include "applib/graphics/gtypes.h"
#include "drivers/ambient_light.h"
#include "drivers/backlight.h"
#include "mfg/mfg_info.h"
#include "os/mutex.h"
#include "popups/timeline/peek.h"
#include "process_management/app_install_manager.h"
#include "process_management/process_manager.h"
#include "services/common/accel_manager.h"
#include "services/common/hrm/hrm_manager.h"
#include "services/common/i18n/i18n.h"
#if CAPABILITY_HAS_ORIENTATION_MANAGER
#include "services/normal/orientation_manager.h"
#endif
#include "services/normal/bluetooth/ble_hrm.h"
#include "services/normal/settings/settings_file.h"
#include "services/normal/timeline/peek.h"
#include "kernel/events.h"
#include "kernel/event_loop.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"
#include "util/uuid.h"

#if CAPABILITY_HAS_HEALTH_TRACKING
#include "services/normal/activity/activity.h"
#include "services/normal/activity/activity_insights.h"
#endif

#include <stdbool.h>

static PebbleMutex *s_mutex;

#define PREF_KEY_CLOCK_24H "clock24h"
static bool s_clock_24h = false;

#define PREF_KEY_CLOCK_TIMEZONE_SOURCE_IS_MANUAL "timezoneSource"
static bool s_clock_timezone_source_is_manual = false;

#define PREF_KEY_CLOCK_PHONE_TIMEZONE_ID "automaticTimezoneID"
static int16_t s_clock_phone_timezone_id = -1;

#define PREF_KEY_UNITS_DISTANCE "unitsDistance"
static uint8_t s_units_distance = UnitsDistance_Miles;

#define PREF_KEY_BACKLIGHT_BEHAVIOUR_DEPRECATED "lightBehaviour"
#define PREF_KEY_BACKLIGHT_ENABLED "lightEnabled"
static bool s_backlight_enabled = true;
#define PREF_KEY_BACKLIGHT_AMBIENT_SENSOR_ENABLED "lightAmbientSensorEnabled"
static bool s_backlight_ambient_sensor_enabled = true;

#define PREF_KEY_BACKLIGHT_TIMEOUT_MS "lightTimeoutMs"
static uint32_t s_backlight_timeout_ms = DEFAULT_BACKLIGHT_TIMEOUT_MS;
#define PREF_KEY_BACKLIGHT_INTENSITY "lightIntensity"
static uint16_t s_backlight_intensity; // default pulled from BOARD_CONFIGs in shell_prefs_init()

#define PREF_KEY_BACKLIGHT_MOTION "lightMotion"
static bool s_backlight_motion_enabled = true;

#define PREF_KEY_MOTION_SENSITIVITY "motionSensitivity"
static uint8_t s_motion_sensitivity = 85; // Default to High

#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
#define PREF_KEY_BACKLIGHT_DYNAMIC_INTENSITY "lightDynamicIntensity"
static bool s_backlight_dynamic_intensity_enabled = true;

#define PREF_KEY_DYNAMIC_BACKLIGHT_MIN_THRESHOLD "dynBacklightMinThreshold"
static uint32_t s_dynamic_backlight_min_threshold = 0; // default set from board config in shell_prefs_init()
#endif

#if CAPABILITY_HAS_ORIENTATION_MANAGER
#define PREF_KEY_DISPLAY_ORIENTATION_LEFT_HANDED "displayOrientationLeftHanded"
static bool s_display_orientation_left = false;
#endif 

#define PREF_KEY_BACKLIGHT_AMBIENT_THRESHOLD "lightAmbientThreshold"
static uint32_t s_backlight_ambient_threshold = 0; // default set from board config in shell_prefs_init()

#define PREF_KEY_STATIONARY "stationaryMode"
#if RELEASE && !PLATFORM_SPALDING
static bool s_stationary_mode_enabled = false;
#else
static bool s_stationary_mode_enabled = true;
#endif

#define PREF_KEY_DEFAULT_WORKER "workerId"
static Uuid s_default_worker = UUID_INVALID_INIT;

// We use "textStyle" to indicate the content size
#define PREF_KEY_TEXT_STYLE "textStyle"
static uint8_t s_text_style = PreferredContentSizeDefault;
#if !UNITTEST
_Static_assert(sizeof(PreferredContentSize) == sizeof(s_text_style),
               "sizeof(PreferredContentSize) grew, pref needs to be migrated!");
#endif

#define PREF_KEY_LANG_ENGLISH "langEnglish"
static bool s_language_english = false;

typedef struct QuickLaunchPreference {
  bool enabled;
  Uuid uuid;
} QuickLaunchPreference;

#define PREF_KEY_QUICK_LAUNCH_UP "qlUp"
#define PREF_KEY_QUICK_LAUNCH_DOWN "qlDown"
#define PREF_KEY_QUICK_LAUNCH_SELECT "qlSelect"
#define PREF_KEY_QUICK_LAUNCH_BACK "qlBack"

static QuickLaunchPreference s_quick_launch_up = {
  .enabled = false,
  .uuid = UUID_INVALID_INIT,
};

static QuickLaunchPreference s_quick_launch_down = {
  .enabled = false,
  .uuid = UUID_INVALID_INIT,
};

static QuickLaunchPreference s_quick_launch_select = {
  .enabled = false,
  .uuid = UUID_INVALID_INIT,
};

static QuickLaunchPreference s_quick_launch_back = {
  .enabled = true,
  .uuid = QUIET_TIME_TOGGLE_UUID,
};

#define PREF_KEY_QUICK_LAUNCH_SINGLE_CLICK_UP "qlSingleClickUp"
#define PREF_KEY_QUICK_LAUNCH_SINGLE_CLICK_DOWN "qlSingleClickDown"
#define PREF_KEY_QUICK_LAUNCH_COMBO_BACK_UP "qlComboBackUp"

static QuickLaunchPreference s_quick_launch_single_click_up = {
  .enabled = true,
  .uuid = UUID_HEALTH_DATA_SOURCE,
};

static QuickLaunchPreference s_quick_launch_single_click_down = {
  .enabled = true,
  .uuid = TIMELINE_UUID_INIT,
};

static QuickLaunchPreference s_quick_launch_combo_back_up = {
  .enabled = false,
  .uuid = UUID_INVALID_INIT,
};

#define PREF_KEY_QUICK_LAUNCH_SETUP_OPENED "qlSetupOpened"
static uint8_t s_quick_launch_setup_opened = 0;

#define PREF_KEY_DEFAULT_WATCHFACE "watchface"
static Uuid s_default_watchface = UUID_INVALID_INIT;

#define PREF_KEY_WELCOME_VERSION "welcomeVersion"
static uint8_t s_welcome_version = 0;

#if CAPABILITY_HAS_HEALTH_TRACKING
#define PREF_KEY_ACTIVITY_PREFERENCES "activityPreferences"
static ActivitySettings s_activity_preferences = ACTIVITY_DEFAULT_PREFERENCES;

#define PREF_KEY_ACTIVITY_ACTIVATED_TIMESTAMP "activityActivated"
static time_t s_activity_activation_timestamp = 0;

#define PREF_KEY_ACTIVITY_ACTIVATION_DELAY_INSIGHT "activityActivationDelayInsights"
static uint32_t s_activity_activation_delay_insight = 0;

#define PREF_KEY_ACTIVITY_HEALTH_APP_OPENED "activityHealthAppOpened"
static uint8_t s_activity_prefs_health_app_opened = 0;

#define PREF_KEY_ACTIVITY_WORKOUT_APP_OPENED "activityWorkoutAppOpened"
static uint8_t s_activity_prefs_workout_app_opened = 0;

#define PREF_KEY_ALARMS_APP_OPENED "alarmsAppOpened"
static uint8_t s_alarms_app_opened = 0;

#define PREF_KEY_ACTIVITY_HRM_PREFERENCES "hrmPreferences"
static ActivityHRMSettings s_activity_hrm_preferences = ACTIVITY_HRM_DEFAULT_PREFERENCES;

#define PREF_KEY_ACTIVITY_HEART_RATE_PREFERENCES "heartRatePreferences"
static HeartRatePreferences s_activity_hr_preferences = ACTIVITY_HEART_RATE_DEFAULT_PREFERENCES;
#endif // CAPABILITY_HAS_HEALTH_TRACKING

#if PLATFORM_SPALDING
#define PREF_KEY_DISPLAY_USER_OFFSET "displayUserOffset"
static GPoint s_display_user_offset = {0, 0};
#define PREF_KEY_SHOULD_PROMPT_DISPLAY_CALIBRATION "promptDisplayCal"
static bool s_should_prompt_display_calibration = true;
#endif

#if CAPABILITY_HAS_TIMELINE_PEEK
#define PREF_KEY_TIMELINE_SETTINGS_OPENED "timelineSettingsOpened"
static uint8_t s_timeline_settings_opened = 0;

#define PREF_KEY_TIMELINE_PEEK_ENABLED "timelineQuickViewEnabled"
static bool s_timeline_peek_enabled = true;

#define PREF_KEY_TIMELINE_PEEK_BEFORE_TIME_M "timelineQuickViewBeforeTimeMin"
static uint16_t s_timeline_peek_before_time_m =
    (TIMELINE_PEEK_DEFAULT_SHOW_BEFORE_TIME_S / SECONDS_PER_MINUTE);
#endif

#define PREF_KEY_COREDUMP_ON_REQUEST "coredumpOnRequest"
#if CAPABILITY_HAS_APP_SCALING
#define PREF_KEY_LEGACY_APP_RENDER_MODE "legacyAppRenderMode"
#endif
static bool s_coredump_on_request_enabled = false;
#if CAPABILITY_HAS_APP_SCALING
static uint8_t s_legacy_app_render_mode = 1; // Default to scaled mode
#endif

#define PREF_KEY_THEME_HIGHLIGHT_COLOR "themeHighlightColor"


static GColor s_theme_highlight_color = GColorVividCerulean;

#define PREF_KEY_MENU_SCROLL_WRAP_AROUND "menuScrollWrapAround"
#define PREF_KEY_MENU_SCROLL_VIBE_BEHAVIOR "menuScrollVibeBehavior"

static bool s_menu_scroll_wrap_around = false;
static MenuScrollVibeBehavior s_menu_scroll_vibe_behavior = MenuScrollNoVibe;

// ============================================================================================
// Handlers for each pref that validate the new setting and store the new value in our globals.
// This handler will be called when the setting is changed from inside the firmware using one of
// the "set" calls or when a pref is changed via a blob_db insert operation from the mobile
// (after we receive the blob_db update event).
//
// If changing of the setting requires more than just setting a global, this handler is the
// place to perform those other actions.
//
// If the the handler gets passed a invalid new value, set its s_* global to a default value,
// and return false. This will trigger a rewrite of the s_* global to the backing file.
//
// Each of these functions MUST be named using the following pattern because they are called
// programmatically via the PREFS_MACRO macro defined below:
//   static bool prv_set_<static_var_name>(<static_var_type> new_value)
static bool prv_set_s_clock_24h(bool *new_value) {
  s_clock_24h = *new_value;
  return true;
}

static bool prv_set_s_clock_timezone_source_is_manual(bool *new_value) {
  s_clock_timezone_source_is_manual = *new_value;
  return true;
}

static bool prv_set_s_clock_phone_timezone_id(int16_t *new_value) {
  s_clock_phone_timezone_id = *new_value;
  return true;
}

static bool prv_set_s_units_distance(uint8_t *new_unit) {
  if (*new_unit >= UnitsDistanceCount) {
    s_units_distance = UnitsDistance_Miles;
    return false;
  }
  s_units_distance = *new_unit;
  return true;
};

static bool prv_set_s_backlight_enabled(bool *enabled) {
  s_backlight_enabled = *enabled;
  return true;
}

static bool prv_set_s_backlight_ambient_sensor_enabled(bool *enabled) {
  s_backlight_ambient_sensor_enabled = *enabled;
  return true;
}

static bool prv_set_s_backlight_timeout_ms(uint32_t *timeout_ms) {
  if (*timeout_ms > 0) {
    s_backlight_timeout_ms = *timeout_ms;
    return true;
  }
  s_backlight_timeout_ms = DEFAULT_BACKLIGHT_TIMEOUT_MS;
  return false;
}

static uint16_t prv_convert_backlight_percent_to_intensity(uint32_t percent_intensity);

static bool prv_set_s_backlight_intensity(uint16_t *intensity) {
  if (*intensity > BACKLIGHT_BRIGHTNESS_OFF) {
    s_backlight_intensity = *intensity;
    return true;
  }
  s_backlight_intensity =
    prv_convert_backlight_percent_to_intensity(BOARD_CONFIG.backlight_on_percent);
  return false;
}

static bool prv_set_s_backlight_motion_enabled(bool *enabled) {
  s_backlight_motion_enabled = *enabled;
  return true;
}

#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
static bool prv_set_s_backlight_dynamic_intensity_enabled(bool *enabled) {
  s_backlight_dynamic_intensity_enabled = *enabled;
  return true;
}

static bool prv_set_s_dynamic_backlight_min_threshold(uint32_t *threshold) {
  // Validate and constrain the threshold
  if (*threshold > AMBIENT_LIGHT_LEVEL_MAX) {
    s_dynamic_backlight_min_threshold = AMBIENT_LIGHT_LEVEL_MAX;
    return false;
  }
  s_dynamic_backlight_min_threshold = *threshold;
  return true;
}
#endif

static bool prv_set_s_motion_sensitivity(uint8_t *sensitivity) {
  // Clamp sensitivity to 0-100 range
  if (*sensitivity > 100) {
    s_motion_sensitivity = 100; // Reset to default if invalid
    return false;
  }
  s_motion_sensitivity = *sensitivity;
  
  // Update accelerometer sensitivity in accel_manager
  // This applies the setting to the hardware
  #if CAPABILITY_HAS_ACCEL_SENSITIVITY
  accel_manager_update_sensitivity(*sensitivity);
  #endif
  
  return true;
}

static bool prv_set_s_backlight_ambient_threshold(uint32_t *threshold) {
  // Validate and constrain the threshold
  if (*threshold > AMBIENT_LIGHT_LEVEL_MAX) {
    s_backlight_ambient_threshold = AMBIENT_LIGHT_LEVEL_MAX;
    return false;
  }
  if (*threshold < 1) {
    s_backlight_ambient_threshold = 1;
    return false;
  }
  s_backlight_ambient_threshold = *threshold;
  // Update the ambient light driver
  ambient_light_set_dark_threshold(*threshold);
  return true;
}

#if CAPABILITY_HAS_ORIENTATION_MANAGER
static bool prv_set_s_display_orientation_left(bool *left) {
  s_display_orientation_left = *left;
  orientation_handle_prefs_changed();
  return true;
}
#endif

static bool prv_set_s_stationary_mode_enabled(bool *enabled) {
  s_stationary_mode_enabled = *enabled;
  return true;
}

static bool prv_set_s_default_worker(Uuid *uuid) {
  s_default_worker = *uuid;
  return true;
}

static bool prv_set_s_text_style(uint8_t *style) {
  s_text_style = *style;
  return true;
}

static bool prv_set_s_language_english(bool *english) {
  s_language_english = *english;
  i18n_enable(!s_language_english);
  return true;
}

// Normalize QuickLaunchPreference: enabled must be false if UUID is invalid
static void prv_normalize_quick_launch_pref(QuickLaunchPreference *pref) {
  if (uuid_is_invalid(&pref->uuid)) {
    pref->enabled = false;
  }
}

static bool prv_set_s_quick_launch_up(QuickLaunchPreference *pref) {
  prv_normalize_quick_launch_pref(pref);
  s_quick_launch_up = *pref;
  return true;
}

static bool prv_set_s_quick_launch_down(QuickLaunchPreference *pref) {
  prv_normalize_quick_launch_pref(pref);
  s_quick_launch_down = *pref;
  return true;
}

static bool prv_set_s_quick_launch_select(QuickLaunchPreference *pref) {
  prv_normalize_quick_launch_pref(pref);
  s_quick_launch_select = *pref;
  return true;
}

static bool prv_set_s_quick_launch_back(QuickLaunchPreference *pref) {
  prv_normalize_quick_launch_pref(pref);
  s_quick_launch_back = *pref;
  return true;
}

static bool prv_set_s_quick_launch_single_click_up(QuickLaunchPreference *pref) {
  prv_normalize_quick_launch_pref(pref);
  s_quick_launch_single_click_up = *pref;
  return true;
}

static bool prv_set_s_quick_launch_single_click_down(QuickLaunchPreference *pref) {
  prv_normalize_quick_launch_pref(pref);
  s_quick_launch_single_click_down = *pref;
  return true;
}

static bool prv_set_s_quick_launch_combo_back_up(QuickLaunchPreference *pref) {
  prv_normalize_quick_launch_pref(pref);
  s_quick_launch_combo_back_up = *pref;
  return true;
}

static bool prv_set_s_quick_launch_setup_opened(uint8_t *version) {
  s_quick_launch_setup_opened = *version;
  return true;
}

static bool prv_set_s_default_watchface(Uuid *uuid) {
  s_default_watchface = *uuid;
  return true;
}

static bool prv_set_s_welcome_version(uint8_t *version) {
  s_welcome_version = *version;
  return true;
}

#if CAPABILITY_HAS_HEALTH_TRACKING
static bool prv_set_s_activity_preferences(ActivitySettings *new_settings) {
  bool invalid_data = false;

  if (new_settings->height_mm <= 0) {
    new_settings->height_mm = ACTIVITY_DEFAULT_HEIGHT_MM;
    invalid_data = true;
  }

  if (new_settings->weight_dag <= 0) {
    new_settings->weight_dag = ACTIVITY_DEFAULT_WEIGHT_DAG;
    invalid_data = true;
  }

  if (!(new_settings->gender == ActivityGenderMale ||
       new_settings->gender == ActivityGenderFemale ||
       new_settings->gender == ActivityGenderOther)) {
    new_settings->gender = ACTIVITY_DEFAULT_GENDER;
    invalid_data = true;
  }

  if (new_settings->age_years <= 0) {
    new_settings->age_years = ACTIVITY_DEFAULT_AGE_YEARS;
    invalid_data = true;
  }

  // Ensure activity is enabled before trying to start tracking. This is necessary because
  // services_set_runlevel() may have already run before activity was initialized (e.g., when
  // time is not valid at boot), which means the enabled_run_level flag was never set.
  // Without this, activity_start_tracking() will fail to start tracking on first boot.
  if (activity_is_initialized()) {
    activity_set_enabled(true);
  }

  if (new_settings->tracking_enabled) {
    activity_start_tracking(false);
  } else {
    activity_stop_tracking();
  }

  s_activity_preferences = *new_settings;

  // If we received invalid data, we return false, so that prefs_private_handle_blob_db_event
  // will rewrite s_activity_preferences to the backing file
  return !invalid_data;
}

static bool prv_set_s_activity_activation_timestamp(time_t *timestamp) {
  s_activity_activation_timestamp = *timestamp;
  return true;
}

static bool prv_set_s_activity_activation_delay_insight(uint32_t *insight_bitmask) {
  s_activity_activation_delay_insight = *insight_bitmask;
  return true;
}

static bool prv_set_s_activity_prefs_health_app_opened(uint8_t *version) {
  s_activity_prefs_health_app_opened = *version;
  return true;
}

static bool prv_set_s_activity_prefs_workout_app_opened(uint8_t *version) {
  s_activity_prefs_workout_app_opened = *version;
  return true;
}

static bool prv_set_s_alarms_app_opened(uint8_t *version) {
  s_alarms_app_opened = *version;
  return true;
}

static bool prv_set_s_activity_hr_preferences(HeartRatePreferences *new_settings) {
  if (new_settings->resting_hr > new_settings->elevated_hr ||
      new_settings->elevated_hr > new_settings->max_hr) {
    return false;
  }
  if (new_settings->zone1_threshold > new_settings->zone2_threshold ||
      new_settings->zone2_threshold > new_settings->zone3_threshold) {
    return false;
  }

  s_activity_hr_preferences = *new_settings;
  return true;
}

static bool prv_set_s_activity_hrm_preferences(ActivityHRMSettings *new_settings) {
  // Set the preference before calling `hrm_manager_enable` because it actually queries
  // for the setting
  s_activity_hrm_preferences = *new_settings;

#if CAPABILITY_HAS_BUILTIN_HRM
  hrm_manager_handle_prefs_changed();
#endif // CAPABILITY_HAS_BUILTIN_HRM
#if BLE_HRM_SERVICE
  ble_hrm_handle_activity_prefs_heart_rate_is_enabled(new_settings->enabled);
#endif // BLE_HRM_SERVICE
  return true;
}
#endif /* CAPABILITY_HAS_HEALTH_TRACKING */


#if PLATFORM_SPALDING
static bool prv_set_s_display_user_offset(GPoint *offset) {
  s_display_user_offset = *offset;
  return true;
}
static bool prv_set_s_should_prompt_display_calibration(bool should_prompt) {
  s_should_prompt_display_calibration = should_prompt;
  return true;
}
#endif

#if CAPABILITY_HAS_TIMELINE_PEEK
static uint8_t prv_set_s_timeline_settings_opened(uint8_t *version) {
  s_timeline_settings_opened = *version;
  return true;
}

// Callback to run timeline_peek_set_enabled on KernelMain task.
// This is needed because animation scheduling requires running on KernelMain or App task,
// but settings sync runs from a different task context.
static void prv_timeline_peek_set_enabled_callback(void *data) {
  timeline_peek_set_enabled(s_timeline_peek_enabled);
}

static bool prv_set_s_timeline_peek_enabled(bool *enabled) {
  s_timeline_peek_enabled = *enabled;
  launcher_task_add_callback(prv_timeline_peek_set_enabled_callback, NULL);
  return true;
}

static bool prv_set_s_timeline_peek_before_time_m(uint16_t *before_time_m) {
  s_timeline_peek_before_time_m = *before_time_m;
  timeline_peek_set_show_before_time(*before_time_m * SECONDS_PER_MINUTE);
  return true;
}
#endif

static bool prv_set_s_coredump_on_request_enabled(bool *enabled) {
  s_coredump_on_request_enabled = *enabled;
  return true;
}

#if CAPABILITY_HAS_APP_SCALING
static bool prv_set_s_legacy_app_render_mode(uint8_t *mode) {
  if (*mode >= LegacyAppRenderModeCount) {
    return false;  // Invalid value
  }
  s_legacy_app_render_mode = *mode;
  return true;
}
#endif

#if PBL_COLOR
// Valid theme colors (unified scheme)
// Must match s_color_definitions in settings_themes.h
static bool prv_is_valid_theme_color(GColor color) {
  // GColorClear means "use default"
  if (color.argb == GColorClear.argb) {
    return true;
  }
  // Valid colors from settings_themes.h
  static const uint8_t valid_colors[] = {
    GColorSunsetOrangeARGB8,        // Red
    GColorChromeYellowARGB8,        // Orange
    GColorYellowARGB8,              // Yellow
    GColorGreenARGB8,               // Green
    GColorCyanARGB8,                // Cyan
    GColorVividCeruleanARGB8,       // Light Blue
    GColorVeryLightBlueARGB8,       // Royal Blue
    GColorLavenderIndigoARGB8,      // Purple
    GColorMagentaARGB8,             // Magenta
    GColorBrilliantRoseARGB8,       // Pink
  };
  for (size_t i = 0; i < ARRAY_LENGTH(valid_colors); i++) {
    if (color.argb == valid_colors[i]) {
      return true;
    }
  }
  return false;
}
#endif

static bool prv_set_s_theme_highlight_color(GColor *color) {
#if PBL_COLOR
  if (!prv_is_valid_theme_color(*color)) {
    PBL_LOG_WRN("Invalid menu highlight color 0x%02x, using default",
            color->argb);
    s_theme_highlight_color = GColorVividCerulean;
    return false;  // Reject invalid value
  }
  s_theme_highlight_color = *color;
#else
  s_theme_highlight_color = GColorBlack;
#endif
  return true;
}

static bool prv_set_s_menu_scroll_wrap_around(bool *enabled) {
  s_menu_scroll_wrap_around = *enabled;
  return true;
}

static bool prv_set_s_menu_scroll_vibe_behavior(MenuScrollVibeBehavior *new_behavior) {
  if (*new_behavior >= MenuScrollVibeBehaviorsCount) {
    s_menu_scroll_vibe_behavior = MenuScrollNoVibe;
    return false;
  }
  s_menu_scroll_vibe_behavior = *new_behavior;
  return true;
}
  
// ------------------------------------------------------------------------------------
// Table of all prefs
typedef bool (*PrefSetHandler)(const void *value, size_t val_len);
typedef struct {
  const char *key;
  void *value;
  uint16_t value_len;
  PrefSetHandler handler;
} PrefsTableEntry;

// The PREFS_DECLARE_HANDLER creates a springboard function with a generic signature
// (of type PrefSetHandler) which simply calls into the specialized function prv_set_<var_name>
// after dereferencing the void* argument using the right type for that pref
#define PREFS_MACRO(name, var) \
  static bool prv_set_ ## var ## _cb(const void *value, size_t val_len) { \
    return prv_set_ ## var ((__typeof__(var) *)value); \
  }
#include "prefs_values.h.inc"
#undef PREFS_MACRO

// Create a time containing the key name and global variable name for each pref
#define PREFS_MACRO(key, var) {key, &var, sizeof(var), prv_set_ ## var ## _cb},
static const PrefsTableEntry s_prefs_table[] = {
#include "prefs_values.h.inc"
};
#undef PREFS_MACRO



// ------------------------------------------------------------------------------------
// FIXME PBL-22272. We back convert this value in
// settings_display.c:prv_get_scaled_brightness() We should really just store
// the percent intensity or a setting level name and leave it up to the light
// module to do the conversion
static uint16_t prv_convert_backlight_percent_to_intensity(uint32_t percent_intensity) {
  return (BACKLIGHT_BRIGHTNESS_MAX * (uint32_t)percent_intensity) / 100;
}


// ------------------------------------------------------------------------------------
static void prv_convert_deprecated_backlight_behaviour_key(SettingsFile *file) {
  // if present, convert deprecated BACKLIGHT_BEHAVIOUR key to the two new separate keys
  if (settings_file_exists(file, PREF_KEY_BACKLIGHT_BEHAVIOUR_DEPRECATED,
                           sizeof(PREF_KEY_BACKLIGHT_BEHAVIOUR_DEPRECATED))) {
    bool temp;
    BacklightBehaviour backlight_behaviour = BacklightBehaviour_Auto;
    settings_file_get(file, PREF_KEY_BACKLIGHT_BEHAVIOUR_DEPRECATED,
                      sizeof(PREF_KEY_BACKLIGHT_BEHAVIOUR_DEPRECATED),
                      &backlight_behaviour, sizeof(backlight_behaviour));
    temp = (backlight_behaviour != BacklightBehaviour_Off);
    settings_file_set(file, PREF_KEY_BACKLIGHT_ENABLED,
                      sizeof(PREF_KEY_BACKLIGHT_ENABLED), &temp, sizeof(temp));
    temp = (backlight_behaviour != BacklightBehaviour_On);
    settings_file_set(file, PREF_KEY_BACKLIGHT_AMBIENT_SENSOR_ENABLED,
                      sizeof(PREF_KEY_BACKLIGHT_AMBIENT_SENSOR_ENABLED), &temp, sizeof(temp));
    settings_file_delete(file, PREF_KEY_BACKLIGHT_BEHAVIOUR_DEPRECATED,
                         sizeof(PREF_KEY_BACKLIGHT_BEHAVIOUR_DEPRECATED));
  }
}


// ------------------------------------------------------------------------------------
void shell_prefs_init(void) {
  s_backlight_intensity =
      prv_convert_backlight_percent_to_intensity(BOARD_CONFIG.backlight_on_percent);
  s_backlight_ambient_threshold = BOARD_CONFIG.ambient_light_dark_threshold;
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
  s_dynamic_backlight_min_threshold = BOARD_CONFIG.dynamic_backlight_min_threshold;
#endif
  // Use board-specific default motion sensitivity if provided (non-zero)
  if (BOARD_CONFIG_ACCEL.accel_config.default_motion_sensitivity != 0) {
    s_motion_sensitivity = BOARD_CONFIG_ACCEL.accel_config.default_motion_sensitivity;
  }
  s_mutex = mutex_create();

  SettingsFile file = {{0}};
  if (settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN) != S_SUCCESS) {
    return;
  }

  prv_convert_deprecated_backlight_behaviour_key(&file);

  // Init state for each pref from our backing store
  uint32_t num_entries = ARRAY_LENGTH(s_prefs_table);
  const PrefsTableEntry *entry = s_prefs_table;
  for (uint32_t i = 0; i < num_entries; i++, entry++) {
    // Keys in the backing store include the null terminator, so we add 1 to key_len
    size_t key_len = strlen(entry->key) + 1;
    if (settings_file_get_len(&file, entry->key, key_len) == entry->value_len) {
      settings_file_get(&file, entry->key, key_len, entry->value, entry->value_len);
    }
  }

  settings_file_close(&file);
  
  // Update the ambient light driver with the loaded threshold value
  ambient_light_set_dark_threshold(s_backlight_ambient_threshold);
  
  // Initialize prefs sync (must be after prefs are loaded)
  prefs_sync_init();

  // Update accelerometer sensitivity with the loaded value
#if CAPABILITY_HAS_ACCEL_SENSITIVITY
  accel_manager_update_sensitivity(s_motion_sensitivity);
#endif
}


// ------------------------------------------------------------------------------------
// Find the PrefsTableEntry for the given key
static const PrefsTableEntry *prv_prefs_entry(const uint8_t *key, size_t key_len) {
  uint32_t num_entries = ARRAY_LENGTH(s_prefs_table);
  const PrefsTableEntry *entry = s_prefs_table;
  for (uint32_t i = 0; i < num_entries; i++, entry++) {
    if (!strncmp((const char *)key, entry->key, key_len)) {
      return entry;
    }
  }
  PBL_LOG_WRN("Unrecognized key: %s", (const char *)key);
  return NULL;
}


// ------------------------------------------------------------------------------------
// Set the backing store for a pref
static bool prv_set_pref_backing(const PrefsTableEntry *entry, const void *value, int value_len) {
  if (value_len != entry->value_len) {
    PBL_LOG_WRN("Attempt to set %s using invalid value_len of %"PRIu32"",
            entry->key, (uint32_t)value_len);
    return false;
  }

  status_t rv = E_ERROR;
  mutex_lock(s_mutex);
  {
    SettingsFile file = {{0}};
    if (settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN) == S_SUCCESS) {
      // Keys in the backing store include the null terminator, so we add 1 to key_len
      rv = settings_file_set(&file, entry->key, strlen(entry->key) + 1, value, value_len);
      if (rv != S_SUCCESS) {
        PBL_LOG_WRN("Failed to set pref '%s' (%"PRIi32")", entry->key, (int32_t)rv);
      }
      settings_file_close(&file);
    }
  }
  mutex_unlock(s_mutex);
  return (rv == S_SUCCESS);
}


// ------------------------------------------------------------------------------------
// Convenience function used to update the state AND set the backing for a pref. This is
// used by the functions below that are called by the firmware to change prefs (i.e.
// shell_prefs_set.*, backlight_set.*, etc.).
static void prv_pref_set(const char* key, const void *value, size_t val_len) {
  // Find the entry for this key
  const PrefsTableEntry *entry = prv_prefs_entry((const uint8_t *)key, strlen(key));

  // validate the key and value length
  PBL_ASSERT(entry != NULL, "Key %s not found", key);
  PBL_ASSERT(val_len == entry->value_len, "Attempt to set %s using invalid value_len of %"PRIu32"",
             entry->key, (uint32_t)val_len);

  // Call the update handler
  bool success = entry->handler(value, val_len);
  PBL_ASSERT(success, "Failure to store new value for %s in settings file", key);

  // Update the backing store
  if (success) {
    prv_set_pref_backing(entry, value, val_len);
  }
}


// ------------------------------------------------------------------------------------
// Exported function used by blob_db API to set the backing store for a specific key
bool prefs_private_write_backing(const uint8_t *key, size_t key_len, const void *value,
                               int value_len) {
  const PrefsTableEntry *entry = prv_prefs_entry(key, key_len);
  if (!entry) {
    return false;
  }

  return prv_set_pref_backing(entry, value, value_len);
}


// ------------------------------------------------------------------------------------
// Exported function used by blob_db API to get the length of a value in our backing store
int prefs_private_get_backing_len(const uint8_t *key, size_t key_len) {
  const PrefsTableEntry *entry = prv_prefs_entry(key, key_len);
  if (!entry) {
    return 0;
  }
  return entry->value_len;
}


// ------------------------------------------------------------------------------------
// Exported function used by blob_db API to read our backing store
bool prefs_private_read_backing(const uint8_t *key, size_t key_len, void *value, int value_len) {
  const PrefsTableEntry *entry = prv_prefs_entry(key, key_len);
  if (!entry) {
    return false;
  }

  if (value_len != entry->value_len) {
    PBL_LOG_WRN("Attempt to read %s using invalid value_len of %"PRIu32"",
            entry->key, (uint32_t)value_len);
    return false;
  }

  bool success = false;
  mutex_lock(s_mutex);
  {
    SettingsFile file = {{0}};
    if (settings_file_open(&file, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN) == S_SUCCESS) {
      // Keys in the backing store include the null terminator
      // Use strlen(entry->key) + 1 to match how it was written, since key_len from
      // BlobDB may or may not include the null terminator
      success = (settings_file_get(&file, entry->key, strlen(entry->key) + 1, value, value_len)
                                   == S_SUCCESS);
      settings_file_close(&file);
    }
  }
  mutex_unlock(s_mutex);
  return success;
}


void prefs_private_lock(void) {
  mutex_lock(s_mutex);
}

void prefs_private_unlock(void) {
  mutex_unlock(s_mutex);
}


// ------------------------------------------------------------------------------------
// Called from KernelMain when we get a blob DB event. We take this opportunity to update the state
// of the given pref
void prefs_private_handle_blob_db_event(PebbleBlobDBEvent *event) {
  if (event->type != BlobDBEventTypeInsert) {
    return;
  }

  const PrefsTableEntry *entry = prv_prefs_entry(event->key, event->key_len);
  if (!entry) {
    return;
  }

  // Read in the updated value from the backing store
  bool success = prefs_private_read_backing(event->key, event->key_len, entry->value,
                                            entry->value_len);
  if (success) {
    // Call the state update handler in case this pref needs to take other action besides
    // just updating the global
    if (!entry->handler(entry->value, entry->value_len)) {
      // If the handler returns false, that means it reset the global back to the default,
      // so we should write the new value to the backing
      prefs_private_write_backing(event->key, event->key_len, entry->value, entry->value_len);
    }

    // Notify UI that a preference changed so it can refresh
    PebbleEvent pref_event = {
      .type = PEBBLE_PREF_CHANGE_EVENT,
      .pref_change = {
        .key = entry->key,
        .key_len = strlen(entry->key) + 1,
      },
    };
    event_put(&pref_event);
  }
}

// ========================================================================================
// Exported functions used by the firmware to read/change a preference.
// IMPORTANT: When implementing the *set* call, be sure to call prv_pref_set(). This does
// two things:
//   1.) It validates that the stored global matches the type of the passed in argument
//   2.) It insures that the flow will also work correctly for setting a pref from the
//        mobile side using a blob_db insert operation.
bool shell_prefs_get_clock_24h_style(void) {
  return s_clock_24h;
}

UnitsDistance shell_prefs_get_units_distance(void) {
  return s_units_distance;
}

void shell_prefs_set_units_distance(UnitsDistance new_unit) {
  uint8_t uint_new_unit = new_unit;
  prv_pref_set(PREF_KEY_UNITS_DISTANCE, &uint_new_unit, sizeof(uint_new_unit));
}

void shell_prefs_set_clock_24h_style(bool is24h) {
  prv_pref_set(PREF_KEY_CLOCK_24H, &is24h, sizeof(is24h));
}

bool shell_prefs_is_timezone_source_manual(void) {
  return s_clock_timezone_source_is_manual;
}

void shell_prefs_set_timezone_source_manual(bool manual) {
  prv_pref_set(PREF_KEY_CLOCK_TIMEZONE_SOURCE_IS_MANUAL, &manual, sizeof(manual));
}

void shell_prefs_set_automatic_timezone_id(int16_t timezone_id) {
  prv_pref_set(PREF_KEY_CLOCK_PHONE_TIMEZONE_ID, &timezone_id, sizeof(timezone_id));
}

int16_t shell_prefs_get_automatic_timezone_id(void) {
  return s_clock_phone_timezone_id;
}

// Emulate the old BacklightBehaviour type for analytics.
// This is a deprecated method and should not be called by new code.
BacklightBehaviour backlight_get_behaviour(void) {
  if (s_backlight_enabled) {
    if (s_backlight_ambient_sensor_enabled) {
      return BacklightBehaviour_Auto;
    } else {
      return BacklightBehaviour_On;
    }
  } else {
    return BacklightBehaviour_Off;
  }
}

bool backlight_is_enabled(void) {
  return s_backlight_enabled;
}

void backlight_set_enabled(bool enabled) {
  prv_pref_set(PREF_KEY_BACKLIGHT_ENABLED, &enabled, sizeof(enabled));
}

bool backlight_is_ambient_sensor_enabled(void) {
#if INFINITE_BACKLIGHT
  return false;
#endif
  return s_backlight_ambient_sensor_enabled;
}

void backlight_set_ambient_sensor_enabled(bool enabled) {
  prv_pref_set(PREF_KEY_BACKLIGHT_AMBIENT_SENSOR_ENABLED, &enabled, sizeof(enabled));
}

uint32_t backlight_get_timeout_ms(void) {
#if INFINITE_BACKLIGHT
  return UINT32_MAX;
#endif
  return s_backlight_timeout_ms;
}

void backlight_set_timeout_ms(uint32_t timeout_ms) {
  prv_pref_set(PREF_KEY_BACKLIGHT_TIMEOUT_MS, &timeout_ms, sizeof(timeout_ms));
}

uint16_t backlight_get_intensity(void) {
  return s_backlight_intensity;
}

uint8_t backlight_get_intensity_percent(void) {
  return (backlight_get_intensity() * 100) / BACKLIGHT_BRIGHTNESS_MAX;
}

void backlight_set_intensity_percent(uint8_t percent_intensity) {
  PBL_ASSERTN(percent_intensity > 0 && percent_intensity <= 100);
  uint16_t intensity = prv_convert_backlight_percent_to_intensity(percent_intensity);
  PBL_ASSERTN(intensity > BACKLIGHT_BRIGHTNESS_OFF);
  prv_pref_set(PREF_KEY_BACKLIGHT_INTENSITY, &intensity, sizeof(intensity));
}

bool backlight_is_motion_enabled(void) {
  return s_backlight_motion_enabled;
}

void backlight_set_motion_enabled(bool enable) {
  prv_pref_set(PREF_KEY_BACKLIGHT_MOTION, &enable, sizeof(enable));
}

#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
bool backlight_is_dynamic_intensity_enabled(void) {
  return s_backlight_dynamic_intensity_enabled;
}

void backlight_set_dynamic_intensity_enabled(bool enable) {
  prv_pref_set(PREF_KEY_BACKLIGHT_DYNAMIC_INTENSITY, &enable, sizeof(enable));
}
#endif

uint8_t shell_prefs_get_motion_sensitivity(void) {
  return s_motion_sensitivity;
}

void shell_prefs_set_motion_sensitivity(uint8_t sensitivity) {
  // Clamp to valid range
  if (sensitivity > 100) {
    sensitivity = 50;
  }
  prv_pref_set(PREF_KEY_MOTION_SENSITIVITY, &sensitivity, sizeof(sensitivity));
}

uint32_t backlight_get_ambient_threshold(void) {
  return s_backlight_ambient_threshold;
}

void backlight_set_ambient_threshold(uint32_t threshold) {
  // Validate threshold is within acceptable range
  if (threshold > AMBIENT_LIGHT_LEVEL_MAX) {
    threshold = AMBIENT_LIGHT_LEVEL_MAX;
  }
  if (threshold < 1) {
    threshold = 1;
  }
  prv_pref_set(PREF_KEY_BACKLIGHT_AMBIENT_THRESHOLD, &threshold, sizeof(threshold));
  // Update the ambient light driver with the new threshold
  ambient_light_set_dark_threshold(threshold);
}

#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
uint32_t backlight_get_dynamic_min_threshold(void) {
  return s_dynamic_backlight_min_threshold;
}

void backlight_set_dynamic_min_threshold(uint32_t threshold) {
  // Validate threshold is within acceptable range
  if (threshold > AMBIENT_LIGHT_LEVEL_MAX) {
    threshold = AMBIENT_LIGHT_LEVEL_MAX;
  }
  if (threshold < 0) {
    threshold = 0;
  }
  // Note: Min threshold should be much smaller than ambient_light_dark_threshold (Zone 2 upper bound)
  // Typically min_threshold is 0-5, while ambient_light_dark_threshold is ~150
  prv_pref_set(PREF_KEY_DYNAMIC_BACKLIGHT_MIN_THRESHOLD, &threshold, sizeof(threshold));
}

#endif

#if CAPABILITY_HAS_ORIENTATION_MANAGER
bool display_orientation_is_left(void) {
  return s_display_orientation_left;
}

void display_orientation_set_left(bool left) {
  prv_pref_set(PREF_KEY_DISPLAY_ORIENTATION_LEFT_HANDED, &left, sizeof(left));
}
#endif

bool shell_prefs_get_stationary_enabled(void) {
  return s_stationary_mode_enabled;
}

void shell_prefs_set_stationary_enabled(bool enabled) {
  prv_pref_set(PREF_KEY_STATIONARY, &enabled, sizeof(enabled));
}

AppInstallId worker_preferences_get_default_worker(void) {
  return app_install_get_id_for_uuid(&s_default_worker);
}

void worker_preferences_set_default_worker(AppInstallId app_id) {
  Uuid uuid;
  app_install_get_uuid_for_install_id(app_id, &uuid);
  prv_pref_set(PREF_KEY_DEFAULT_WORKER, &uuid, sizeof(uuid));
}

bool quick_launch_is_enabled(ButtonId button) {
  switch (button) {
    case BUTTON_ID_UP:
      return s_quick_launch_up.enabled;
    case BUTTON_ID_DOWN:
      return s_quick_launch_down.enabled;
    case BUTTON_ID_SELECT:
      return s_quick_launch_select.enabled;
    case BUTTON_ID_BACK:
      return s_quick_launch_back.enabled;
    case NUM_BUTTONS:
      break;
  }
  return false;
}

AppInstallId quick_launch_get_app(ButtonId button) {
  Uuid *uuid = NULL;
  switch (button) {
    case BUTTON_ID_UP:
      uuid = &s_quick_launch_up.uuid;
      break;
    case BUTTON_ID_DOWN:
      uuid = &s_quick_launch_down.uuid;
      break;
    case BUTTON_ID_SELECT:
      uuid = &s_quick_launch_select.uuid;
      break;
    case BUTTON_ID_BACK:
      uuid = &s_quick_launch_back.uuid;
      break;
    case NUM_BUTTONS:
      break;
  }
  PBL_ASSERTN(uuid);
  return app_install_get_id_for_uuid(uuid);
}

void quick_launch_set_app(ButtonId button, AppInstallId app_id) {
  QuickLaunchPreference pref = (QuickLaunchPreference) {
    .enabled = true,
  };
  app_install_get_uuid_for_install_id(app_id, &pref.uuid);

  const char *key = NULL;
  switch (button) {
    case BUTTON_ID_UP:
      key = PREF_KEY_QUICK_LAUNCH_UP;
      break;
    case BUTTON_ID_DOWN:
      key = PREF_KEY_QUICK_LAUNCH_DOWN;
      break;
    case BUTTON_ID_SELECT:
      key = PREF_KEY_QUICK_LAUNCH_SELECT;
      break;
    case BUTTON_ID_BACK:
      key = PREF_KEY_QUICK_LAUNCH_BACK;
      break;
    case NUM_BUTTONS:
      break;
  }
  PBL_ASSERTN(key);
  prv_pref_set(key, &pref, sizeof(pref));
}

void quick_launch_set_enabled(ButtonId button, bool enabled) {
  QuickLaunchPreference pref;

  const char *key = NULL;
  switch (button) {
    case BUTTON_ID_UP:
      pref = s_quick_launch_up;
      key = PREF_KEY_QUICK_LAUNCH_UP;
      break;
    case BUTTON_ID_DOWN:
      pref = s_quick_launch_down;
      key = PREF_KEY_QUICK_LAUNCH_DOWN;
      break;
    case BUTTON_ID_SELECT:
      pref = s_quick_launch_select;
      key = PREF_KEY_QUICK_LAUNCH_SELECT;
      break;
    case BUTTON_ID_BACK:
      pref = s_quick_launch_back;
      key = PREF_KEY_QUICK_LAUNCH_BACK;
      break;
    case NUM_BUTTONS:
      break;
  }
  PBL_ASSERTN(key);
  pref.enabled = enabled;
  prv_pref_set(key, &pref, sizeof(pref));
}

void quick_launch_set_quick_launch_setup_opened(uint8_t version) {
  if (s_quick_launch_setup_opened != version) {
    s_quick_launch_setup_opened = version;
    prv_pref_set(PREF_KEY_QUICK_LAUNCH_SETUP_OPENED, &s_quick_launch_setup_opened,
                 sizeof(s_quick_launch_setup_opened));
  }
}

uint8_t quick_launch_get_quick_launch_setup_opened(void) {
  return s_quick_launch_setup_opened;
}

bool quick_launch_single_click_is_enabled(ButtonId button) {
    switch (button) {
      case BUTTON_ID_UP:
        return s_quick_launch_single_click_up.enabled;
      case BUTTON_ID_DOWN:
        return s_quick_launch_single_click_down.enabled;
      case BUTTON_ID_SELECT:
      case BUTTON_ID_BACK:
      case NUM_BUTTONS:
        break;
    }
    return false;
}

AppInstallId quick_launch_single_click_get_app(ButtonId button) {
  Uuid *uuid = NULL;
  switch (button) {
    case BUTTON_ID_UP:
      uuid = &s_quick_launch_single_click_up.uuid;
      break;
    case BUTTON_ID_DOWN:
      uuid = &s_quick_launch_single_click_down.uuid;
      break;
    default:
      PBL_ASSERTN(0); // Should not reach here: invalid button id
      break;
  }
  return app_install_get_id_for_uuid(uuid);
}

void quick_launch_single_click_set_app(ButtonId button, AppInstallId app_id) {
  QuickLaunchPreference pref = (QuickLaunchPreference) {
    .enabled = true,
  };
  app_install_get_uuid_for_install_id(app_id, &pref.uuid);

  const char *key = NULL;
  switch (button) {
    case BUTTON_ID_UP:
      key = PREF_KEY_QUICK_LAUNCH_SINGLE_CLICK_UP;
      break;
    case BUTTON_ID_DOWN:
      key = PREF_KEY_QUICK_LAUNCH_SINGLE_CLICK_DOWN;
      break;
    default:
      PBL_ASSERTN(0); // Should not reach here: invalid button id
      break;
  }
  prv_pref_set(key, &pref, sizeof(pref));
}

void quick_launch_single_click_set_enabled(ButtonId button, bool enabled) {
  QuickLaunchPreference pref;

  const char *key = NULL;
  switch (button) {
    case BUTTON_ID_UP:
      pref = s_quick_launch_single_click_up;
      key = PREF_KEY_QUICK_LAUNCH_SINGLE_CLICK_UP;
      break;
    case BUTTON_ID_DOWN:
      pref = s_quick_launch_single_click_down;
      key = PREF_KEY_QUICK_LAUNCH_SINGLE_CLICK_DOWN;
      break;
    default:
      PBL_ASSERTN(0); // Should not reach here: invalid button id
      break;
  }
  pref.enabled = enabled;
  prv_pref_set(key, &pref, sizeof(pref));
}

bool quick_launch_combo_back_up_is_enabled(void) {
  return s_quick_launch_combo_back_up.enabled;
}

AppInstallId quick_launch_combo_back_up_get_app(void) {
  return app_install_get_id_for_uuid(&s_quick_launch_combo_back_up.uuid);
}

void quick_launch_combo_back_up_set_app(AppInstallId app_id) {
  QuickLaunchPreference pref = (QuickLaunchPreference) {
    .enabled = true,
  };
  app_install_get_uuid_for_install_id(app_id, &pref.uuid);
  prv_pref_set(PREF_KEY_QUICK_LAUNCH_COMBO_BACK_UP, &pref, sizeof(pref));
}

void quick_launch_combo_back_up_set_enabled(bool enabled) {
  QuickLaunchPreference pref = s_quick_launch_combo_back_up;
  pref.enabled = enabled;
  prv_pref_set(PREF_KEY_QUICK_LAUNCH_COMBO_BACK_UP, &pref, sizeof(pref));
}

void watchface_set_default_install_id(AppInstallId app_id) {
  Uuid uuid;
  app_install_get_uuid_for_install_id(app_id, &uuid);
  prv_pref_set(PREF_KEY_DEFAULT_WATCHFACE, &uuid, sizeof(uuid));
}

void welcome_set_welcome_version(uint8_t version) {
  if (s_welcome_version != version) {
    s_welcome_version = version;
    prv_pref_set(PREF_KEY_WELCOME_VERSION, &version, sizeof(version));
  }
}

uint8_t welcome_get_welcome_version(void) {
  return s_welcome_version;
}

static bool prv_set_default_any_watchface_enumerate_callback(AppInstallEntry *entry, void *data) {
  if (!app_install_entry_is_watchface(entry)
      || app_install_entry_is_hidden(entry)) {
    return true; // continue search
  }

  watchface_set_default_install_id(entry->install_id);
  return false;
}

AppInstallId watchface_get_default_install_id(void) {
  AppInstallId app_id = app_install_get_id_for_uuid(&s_default_watchface);
  AppInstallEntry entry;
  if (app_id == INSTALL_ID_INVALID ||
      !app_install_get_entry_for_install_id(app_id, &entry) ||
      !app_install_entry_is_watchface(&entry)) {
    app_install_enumerate_entries(
        prv_set_default_any_watchface_enumerate_callback, NULL);
    app_id = app_install_get_id_for_uuid(&s_default_watchface);
  }
  return app_id;
}

void system_theme_set_content_size(PreferredContentSize content_size) {
  if (content_size >= NumPreferredContentSizes) {
    PBL_LOG_WRN("Ignoring attempt to set content size to invalid size %d",
            content_size);
    return;
  }
  const uint8_t content_size_uint = content_size;
  prv_pref_set(PREF_KEY_TEXT_STYLE, &content_size_uint, sizeof(content_size_uint));
}

PreferredContentSize system_theme_get_content_size(void) {
  return system_theme_convert_host_content_size_to_runtime_platform(
      (PreferredContentSize)s_text_style);
}

bool shell_prefs_get_language_english(void) {
  return s_language_english;
}

void shell_prefs_set_language_english(bool english) {
  prv_pref_set(PREF_KEY_LANG_ENGLISH, &english, sizeof(english));
}

void shell_prefs_toggle_language_english(void) {
  shell_prefs_set_language_english(!shell_prefs_get_language_english());
}

#if CAPABILITY_HAS_HEALTH_TRACKING
static void prv_activity_pref_set(void) {
  prv_pref_set(PREF_KEY_ACTIVITY_PREFERENCES, &s_activity_preferences,
               sizeof(s_activity_preferences));
}

time_t activity_prefs_get_activation_time(void) {
  return s_activity_activation_timestamp;
}

void activity_prefs_set_activated(void) {
  if (s_activity_activation_timestamp == 0) {
    s_activity_activation_timestamp = rtc_get_time();
    prv_pref_set(PREF_KEY_ACTIVITY_ACTIVATED_TIMESTAMP, &s_activity_activation_timestamp,
                 sizeof(s_activity_activation_timestamp));
  }
}

bool activity_prefs_has_activation_delay_insight_fired(ActivationDelayInsightType type) {
  return (s_activity_activation_delay_insight & (1 << type));
}

void activity_prefs_set_activation_delay_insight_fired(ActivationDelayInsightType type) {
  s_activity_activation_delay_insight |= (1 << type);
  prv_pref_set(PREF_KEY_ACTIVITY_ACTIVATION_DELAY_INSIGHT, &s_activity_activation_delay_insight,
               sizeof(s_activity_activation_delay_insight));
}

uint8_t activity_prefs_get_health_app_opened_version(void) {
  return s_activity_prefs_health_app_opened;
}

void activity_prefs_set_health_app_opened_version(uint8_t version) {
  if (s_activity_prefs_health_app_opened != version) {
    s_activity_prefs_health_app_opened = version;
    prv_pref_set(PREF_KEY_ACTIVITY_HEALTH_APP_OPENED, &s_activity_prefs_health_app_opened,
                 sizeof(s_activity_prefs_health_app_opened));
  }
}

uint8_t activity_prefs_get_workout_app_opened_version(void) {
  return s_activity_prefs_workout_app_opened;
}

void activity_prefs_set_workout_app_opened_version(uint8_t version) {
  if (s_activity_prefs_workout_app_opened != version) {
    s_activity_prefs_workout_app_opened = version;
    prv_pref_set(PREF_KEY_ACTIVITY_WORKOUT_APP_OPENED, &s_activity_prefs_workout_app_opened,
                 sizeof(s_activity_prefs_workout_app_opened));
  }
}

bool activity_prefs_activity_insights_are_enabled(void) {
  return s_activity_preferences.activity_insights_enabled;
}

void activity_prefs_activity_insights_set_enabled(bool enable) {
  s_activity_preferences.activity_insights_enabled = enable;
  prv_activity_pref_set();
}

bool activity_prefs_sleep_insights_are_enabled(void) {
  return s_activity_preferences.sleep_insights_enabled;
}

void activity_prefs_sleep_insights_set_enabled(bool enable) {
  s_activity_preferences.sleep_insights_enabled = enable;
  prv_activity_pref_set();
}

bool activity_prefs_tracking_is_enabled(void) {
  return s_activity_preferences.tracking_enabled;
}

void activity_prefs_tracking_set_enabled(bool enable) {
  s_activity_preferences.tracking_enabled = enable;
  prv_activity_pref_set();
}

void activity_prefs_set_height_mm(uint16_t height_mm) {
  s_activity_preferences.height_mm = height_mm;
  prv_activity_pref_set();
}

uint16_t activity_prefs_get_height_mm(void) {
  return s_activity_preferences.height_mm;
}

void activity_prefs_set_weight_dag(uint16_t weight_dag) {
  s_activity_preferences.weight_dag = weight_dag;
  prv_activity_pref_set();
}

uint16_t activity_prefs_get_weight_dag(void) {
  return s_activity_preferences.weight_dag;
}

void activity_prefs_set_gender(ActivityGender gender) {
  s_activity_preferences.gender = gender;
  prv_activity_pref_set();
}

ActivityGender activity_prefs_get_gender(void) {
  return s_activity_preferences.gender;
}

void activity_prefs_set_age_years(uint8_t age_years) {
  s_activity_preferences.age_years = age_years;
  prv_activity_pref_set();
}

uint8_t activity_prefs_get_age_years(void) {
  return s_activity_preferences.age_years;
}

uint8_t activity_prefs_heart_get_resting_hr(void) {
  return s_activity_hr_preferences.resting_hr;
}

uint8_t activity_prefs_heart_get_elevated_hr(void) {
  return s_activity_hr_preferences.elevated_hr;
}

uint8_t activity_prefs_heart_get_max_hr(void) {
  return s_activity_hr_preferences.max_hr;
}

uint8_t activity_prefs_heart_get_zone1_threshold(void) {
  return s_activity_hr_preferences.zone1_threshold;
}

uint8_t activity_prefs_heart_get_zone2_threshold(void) {
  return s_activity_hr_preferences.zone2_threshold;
}

uint8_t activity_prefs_heart_get_zone3_threshold(void) {
  return s_activity_hr_preferences.zone3_threshold;
}

bool activity_prefs_heart_rate_is_enabled(void) {
  return s_activity_hrm_preferences.enabled;
}

void alarm_prefs_set_alarms_app_opened(uint8_t version) {
  if (s_alarms_app_opened != version) {
    s_alarms_app_opened = version;
    prv_pref_set(PREF_KEY_ALARMS_APP_OPENED, &s_alarms_app_opened, sizeof(s_alarms_app_opened));
  }
}

uint8_t alarm_prefs_get_alarms_app_opened(void) {
  return s_alarms_app_opened;
}
#endif /* CAPABILITY_HAS_HEALTH_TRACKING */

#if PLATFORM_SPALDING
void shell_prefs_set_display_offset(GPoint offset) {
  const GPoint user_offset = gpoint_sub(offset, mfg_info_get_disp_offsets());
  prv_pref_set(PREF_KEY_DISPLAY_USER_OFFSET, &user_offset, sizeof(user_offset));
}

GPoint shell_prefs_get_display_offset(void) {
  return gpoint_add(s_display_user_offset, mfg_info_get_disp_offsets());
}

void shell_prefs_display_offset_init(void) {
  display_set_offset(shell_prefs_get_display_offset());
}

bool shell_prefs_should_prompt_display_calibration(void) {
  return s_should_prompt_display_calibration;
}

void shell_prefs_set_should_prompt_display_calibration(bool should_prompt) {
  s_should_prompt_display_calibration = should_prompt;
  prv_pref_set(PREF_KEY_SHOULD_PROMPT_DISPLAY_CALIBRATION, &s_should_prompt_display_calibration,
               sizeof(s_should_prompt_display_calibration));
}
#endif

#if CAPABILITY_HAS_TIMELINE_PEEK
void timeline_prefs_set_settings_opened(uint8_t version) {
  prv_pref_set(PREF_KEY_TIMELINE_SETTINGS_OPENED, &version, sizeof(version));
}

uint8_t timeline_prefs_get_settings_opened(void) {
  return s_timeline_settings_opened;
}

void timeline_peek_prefs_set_enabled(bool enabled) {
  prv_pref_set(PREF_KEY_TIMELINE_PEEK_ENABLED, &enabled, sizeof(enabled));
}

bool timeline_peek_prefs_get_enabled(void) {
  return s_timeline_peek_enabled;
}

void timeline_peek_prefs_set_before_time(uint16_t before_time_m) {
  prv_pref_set(PREF_KEY_TIMELINE_PEEK_BEFORE_TIME_M, &before_time_m, sizeof(before_time_m));
}

uint16_t timeline_peek_prefs_get_before_time(void) {
  return s_timeline_peek_before_time_m;
}
#else
uint16_t timeline_peek_prefs_get_before_time(void) {
  return TIMELINE_PEEK_DEFAULT_SHOW_BEFORE_TIME_S;
}
#endif

bool shell_prefs_can_coredump_on_request(void) {
  return s_coredump_on_request_enabled;
}

void shell_prefs_set_coredump_on_request(bool enabled) {
  prv_pref_set(PREF_KEY_COREDUMP_ON_REQUEST, &enabled, sizeof(enabled));
}

#if CAPABILITY_HAS_APP_SCALING
LegacyAppRenderMode shell_prefs_get_legacy_app_render_mode(void) {
  return (LegacyAppRenderMode)s_legacy_app_render_mode;
}

void shell_prefs_set_legacy_app_render_mode(LegacyAppRenderMode mode) {
  uint8_t mode_value = (uint8_t)mode;
  prv_pref_set(PREF_KEY_LEGACY_APP_RENDER_MODE, &mode_value, sizeof(mode_value));
}
#endif

GColor shell_prefs_get_theme_highlight_color(void){
  #if !PBL_COLOR
    return GColorBlack;
  #endif
  return s_theme_highlight_color;
}

void shell_prefs_set_theme_highlight_color(GColor color) {
  prv_pref_set(PREF_KEY_THEME_HIGHLIGHT_COLOR, &color, sizeof(GColor));
}

bool shell_prefs_get_menu_scroll_wrap_around_enable(void) {
  return s_menu_scroll_wrap_around;
}

void shell_prefs_set_menu_scroll_wrap_around_enable(bool enable) {
  prv_pref_set(PREF_KEY_MENU_SCROLL_WRAP_AROUND, &enable, sizeof(bool));
}

MenuScrollVibeBehavior shell_prefs_get_menu_scroll_vibe_behavior(void) {
  return s_menu_scroll_vibe_behavior;
}

void shell_prefs_set_menu_scroll_vibe_behavior(MenuScrollVibeBehavior behavior) {
  prv_pref_set(PREF_KEY_MENU_SCROLL_VIBE_BEHAVIOR, &behavior, sizeof(MenuScrollVibeBehavior));
}