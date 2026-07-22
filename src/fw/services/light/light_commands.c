/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/prompt.h"
#include "drivers/backlight.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "pbl/services/light.h"
#include "shell/prefs.h"
#include "util/time/time.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined(CONFIG_BACKLIGHT_HAS_COLOR) && defined(CONFIG_BACKLIGHT_QEMU_COLOR)

static uint16_t prv_current_minute_of_day(void) {
  struct tm local;
  const time_t now = rtc_get_time();
  localtime_r(&now, &local);

  return local.tm_hour * MINUTES_PER_HOUR + local.tm_min;
}

static bool prv_is_daytime(uint16_t minute, uint16_t sunrise, uint16_t sunset) {
  if (sunrise == sunset) {
    return true;
  } else if (sunrise < sunset) {
    return minute >= sunrise && minute < sunset;
  }

  return minute >= sunrise || minute < sunset;
}

static void prv_put_pref_change_event(const char *key) {
  PebbleEvent event = {
    .type = PEBBLE_PREF_CHANGE_EVENT,
    .pref_change = {
      .key = key,
      .key_len = strlen(key) + 1,
    },
  };
  event_put(&event);
}

static void prv_status(void) {
  const uint16_t sunrise = backlight_get_sunrise_minute();
  const uint16_t sunset = backlight_get_sunset_minute();
  const uint16_t minute = prv_current_minute_of_day();

  char buffer[128];
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "enabled=%d now=%u day=%d sunrise=%u sunset=%u",
                           (int)backlight_day_night_color_is_enabled(), (unsigned)minute,
                           (int)prv_is_daytime(minute, sunrise, sunset), (unsigned)sunrise,
                           (unsigned)sunset);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "day=%06"PRIx32" night=%06"PRIx32" current=%06"PRIx32,
                           backlight_get_default_color(), backlight_get_night_color(),
                           backlight_get_color());
}

static void prv_start(void) {
  const uint16_t sunrise = prv_current_minute_of_day();
  const uint16_t sunset = (sunrise + 1) % MINUTES_PER_DAY;

  light_set_system_color();
  backlight_set_default_color(BACKLIGHT_COLOR_GREEN);
  backlight_set_night_color(BACKLIGHT_COLOR_RED);
  backlight_set_sunrise_minute(sunrise);
  backlight_set_sunset_minute(sunset);
  backlight_day_night_color_set_enabled(true);
  prv_put_pref_change_event("lightColorDayNightEnabled");
  light_enable(true);

  char buffer[128];
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "day/night test armed: green now, red at minute %u",
                           (unsigned)sunset);
  prv_status();
}

static void prv_off(void) {
  backlight_day_night_color_set_enabled(false);
  prv_put_pref_change_event("lightColorDayNightEnabled");
  light_set_system_color();
  light_enable(true);

  prompt_send_response("day/night test disabled");
  prv_status();
}

void command_backlight_day_night_test(const char *arg) {
  if (strcmp(arg, "start") == 0) {
    prv_start();
  } else if (strcmp(arg, "status") == 0) {
    prv_status();
  } else if (strcmp(arg, "off") == 0) {
    prv_off();
  } else {
    prompt_send_response("usage: backlight daynight <start|status|off>");
  }
}

#endif
