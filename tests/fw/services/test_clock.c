/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/clock.h"
#include "services/normal/timezone_database.h"
#include "services/normal/filesystem/pfs.h"
#include "resource/resource_ids.auto.h"
#include "flash_region/flash_region_s29vs.h"
#include "resource/resource.h"
#include "resource/resource_version.auto.h"

// Fixture
////////////////////////////////////////////////////////////////
#include "fixtures/load_test_resources.h"

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_rtc.h"
#include "fake_events.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

// Stubs
////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_language_ui.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_regular_timer.h"
#include "stubs_serial.h"
#include "stubs_session.h"
#include "stubs_sleep.h"
#include "stubs_system_reset.h"
#include "stubs_task_watchdog.h"
#include "stubs_memory_layout.h"

static bool s_prefs_24h_style;

#define TIMEZONE_FIXTURE_PATH "timezones"

// Setup
/////////////////////////
void test_clock__initialize(void) {
  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false);
  resource_init();
}

extern void prv_update_time_info_and_generate_event(time_t *t, TimezoneInfo *tz_info);
void clock_set_time(time_t t) {
  prv_update_time_info_and_generate_event(&t, NULL);
}

static void prv_clock_reset(int32_t gmtoff) {
  TimezoneInfo tzinfo = {{0}};
  tzinfo.dst_id = 0;
  tzinfo.dst_start = tzinfo.dst_end = 0;
  tzinfo.tm_gmtoff = gmtoff;
  tzinfo.timezone_id = 0;
  rtc_set_timezone(&tzinfo);
  clock_init();
  clock_set_24h_style(false);
}
static void prv_set_current_time(struct tm new_time) {
  time_t secs = mktime(&new_time);
  rtc_set_time(secs);
}

// Fakes
///////////////////////////
void notifications_migrate_timezone(int utc_diff) {
}
void wakeup_migrate_timezone(int utc_diff) {
}
bool shell_prefs_get_clock_24h_style(void) {
  return s_prefs_24h_style;
}
void shell_prefs_set_clock_24h_style(bool is_24h_style) {
  s_prefs_24h_style = is_24h_style;
}
bool shell_prefs_is_timezone_source_manual(void) {
  return false;
}
void shell_prefs_set_timezone_source_manual(bool manual) {
}
int16_t shell_prefs_get_automatic_timezone_id(void) {
  return -1;
}
void shell_prefs_set_automatic_timezone_id(int16_t timezone_id) {
}
bool shell_prefs_get_language_english(void) {
  return false;
}
void shell_prefs_set_language_english(bool english) {
}
void sys_localtime_r(time_t const *t, struct tm *lcltime) {
  localtime_r(t, lcltime);
}
void sys_gmtime_r(time_t const *t, struct tm *lcltime) {
  gmtime_r(t, lcltime);
}
void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

// Tests
///////////////////////////
void test_clock__basic_no_timezone_set_time(void) {
  s_prefs_24h_style = false;
  fake_rtc_init(0, 0);
  rtc_timezone_clear();
  clock_init();

  static const time_t jan1st_noon_2005 = 1104580800;
  clock_set_time(jan1st_noon_2005);
  cl_assert_equal_i(rtc_get_time(), jan1st_noon_2005);
}

void test_clock__basic_timezone_gmtoffset(void) {
  s_prefs_24h_style = false;
  fake_rtc_init(0, 0);
  rtc_timezone_clear();

  static const time_t jan1st_noon_2005 = 1104580800;
  static const int32_t min_gmtoff = -12 * SECONDS_PER_HOUR;
  static const int32_t max_gmtoff =  12 * SECONDS_PER_HOUR;
  static const int32_t gmtoff_slide = SECONDS_PER_MINUTE;

  TimezoneInfo tzinfo = {{0}};
  strcpy(tzinfo.tm_zone, "UNK");
  tzinfo.dst_id = 0;
  tzinfo.dst_start = tzinfo.dst_end = 0;
  tzinfo.timezone_id = -1;
  for (int32_t gmtoff = min_gmtoff; gmtoff <= max_gmtoff; gmtoff += gmtoff_slide) {
    prv_clock_reset(0);

    tzinfo.tm_gmtoff = gmtoff;
    rtc_set_timezone(&tzinfo);
    clock_init();
    clock_set_time(jan1st_noon_2005);
    cl_assert_equal_i(rtc_get_time(), jan1st_noon_2005);
    cl_assert_equal_i(time_utc_to_local(rtc_get_time()), jan1st_noon_2005 + gmtoff);
  }
}

void test_clock__basic_timezone_dst(void) {
  s_prefs_24h_style = false;
  fake_rtc_init(0, 0);
  rtc_timezone_clear();

  static const time_t jan1st_noon_2005 = 1104580800;
  static const int32_t min_dstoff = -12 * SECONDS_PER_HOUR;
  static const int32_t max_dstoff =  12 * SECONDS_PER_HOUR;
  static const int32_t dstoff_slide = SECONDS_PER_MINUTE;
  static const int32_t dstrange = SECONDS_PER_HOUR;
  TimezoneInfo tzinfo = {{0}};
  strcpy(tzinfo.tm_zone, "UNK");
  tzinfo.dst_id = 0;
  tzinfo.tm_gmtoff = 0;
  tzinfo.timezone_id = -1;
  for (int32_t dstoff = min_dstoff; dstoff <= max_dstoff; dstoff += dstoff_slide) {
    prv_clock_reset(0);

    tzinfo.dst_start = dstoff;
    tzinfo.dst_end = tzinfo.dst_start + dstrange;
    rtc_set_timezone(&tzinfo);
    clock_init();
    clock_set_time(jan1st_noon_2005);
    cl_assert_equal_i(rtc_get_time(), jan1st_noon_2005);
    if (tzinfo.dst_start <= rtc_get_time() && rtc_get_time() < tzinfo.dst_end) {
      cl_assert_equal_i(time_utc_to_local(rtc_get_time()), jan1st_noon_2005 + time_get_dstoffset());
    } else {
      cl_assert_equal_i(time_utc_to_local(rtc_get_time()), jan1st_noon_2005);
    }
  }
}

#define DST_ID_COUNT 36
static const time_t s_dst_correct_values[DST_ID_COUNT][3] = {
  /* No DST: 0 ~ 0, GMT+0 */
  [ 0]={          0,  0, 0 },
  /* AN (New South Wales) [Australia/Sydney]
  Rule  AN  2008  max - Apr Sun>=1  2:00s 0 S
  Rule  AN  2008  max - Oct Sun>=1  2:00s 1:00  D
   Oct  4th 2014 16:00 UTC ~ Apr  4th 2015 16:00 UTC, GMT+10 */
  [ 1]={             1412438400,               1428163200, 10 * SECONDS_PER_HOUR },
  /* AS (South Australia) [Australia/Adelaide]
  Rule  AS  2008  max - Apr Sun>=1  2:00s 0 S
  Rule  AS  2008  max - Oct Sun>=1  2:00s 1:00  D
   Oct  4th 2014 16:30 UTC ~ Apr  4th 2015 16:30 UTC, GMT+9.5 */
  [ 2]={             1412440200,               1428165000, 9.5 * SECONDS_PER_HOUR },
  /* AT (Tasmania) [Australia/Hobart]
  Rule  AT  2008  max - Apr Sun>=1  2:00s 0 S
  Rule  AT  2001  max - Oct Sun>=1  2:00s 1:00  D
   Oct  4th 2014 16:00 UTC ~ Apr  4th 2015 16:00 UTC, GMT+10 */
  [ 3]={             1412438400,               1428163200, 10 * SECONDS_PER_HOUR },
  /* AV (Victoria) [Australia/Melbourne]
  Rule  AV  2008  max - Apr Sun>=1  2:00s 0 S
  Rule  AV  2008  max - Oct Sun>=1  2:00s 1:00  D
   Oct  4th 2014 16:00 UTC ~ Apr  4th 2015 16:00 UTC, GMT+10 */
  [ 4]={             1412438400,               1428163200, 10 * SECONDS_PER_HOUR },

  /* Azer (Azerbaijan) [Asia/Baku]
   * Azerbaijan has abandoned DST */
  [ 5]={                      0,                        0, 4 * SECONDS_PER_HOUR },

  /* Brazil (Brazil) [America/Sao_Paulo]
  Rule  Brazil  2008  max - Oct Sun>=15 0:00  1:00  S
  *Rule  Brazil  2012  only  - Feb Sun>=22 0:00  0 -
  *Rule  Brazil  2013  2014  - Feb Sun>=15 0:00  0 -
  * THESE TWO RULES REPEAT FROM NOW ONWARDS
   Oct 19th 2014 03:00 UTC ~ Feb 22nd 2015 02:00 UTC, GMT-3 */
  [ 6]={             1413687600,               1424570400,-3 * SECONDS_PER_HOUR },

  /* C-Eur (Central Europe) [Nowhere actually uses this anymore lol]
  Rule  C-Eur 1981  max - Mar lastSun  2:00s  1:00  S
  Rule  C-Eur 1996  max - Oct lastSun  2:00s  0 -
  * For all intents and purposes, this is the same as EU.
   Mar 29th 2015 01:00 UTC ~ Oct 25th 2015 01:00 UTC, GMT+1 */
  [ 7]={             1427590800,               1445734800, 1 * SECONDS_PER_HOUR },

  /* Canada (Canada) [America/Toronto]
  Rule  Canada  2007  max - Mar Sun>=8  2:00  1:00  D
  Rule  Canada  2007  max - Nov Sun>=1  2:00  0 S
   Mar  8th 2015 07:00 UTC ~ Nov  1st 2015 06:00 UTC, GMT-5 */
  [ 8]={             1425798000,               1446357600,-5 * SECONDS_PER_HOUR },

  /* Chatham (Chatham) [Pacific/Chatham]
  Rule  Chatham 2007  max - Sep lastSun 2:45s 1:00  D
  Rule  Chatham 2008  max - Apr Sun>=1  2:45s 0 S
   Sep 27th 2014 14:00 UTC ~ Apr  4th 2015 14:00 UTC, GMT+12.75 */
  [ 9]={             1411826400,               1428156000,12.75 * SECONDS_PER_HOUR },

  /* ChileAQ (Chile Antarctica Bases) [Antarctica/Palmer]
  Rule  Chile 2012  max - Apr Sun>=23 3:00u 0 -
  Rule  Chile 2012  max - Sep Sun>=2  4:00u 1:00  S
  * ChileAQ is literally the same as Chile now.
  * From Chile: Actually, Chile no longer observes DST, so this is no longer used.
   Sep  7th 2014 04:00 UTC ~ Apr 26th 2015 03:00 UTC, GMT-4 */
  [10]={                      0,                        0,-4 * SECONDS_PER_HOUR },

  /* Cuba (Cuba) [America/Havana]
  Rule  Cuba  2012  max - Nov Sun>=1  0:00s 0 S
  Rule  Cuba  2013  max - Mar Sun>=8  0:00s 1:00  D
   Mar  8th 2015 05:00 UTC ~ Nov  1st 2015 05:00 UTC, GMT-5 */
  [11]={             1425790800,               1446354000,-5 * SECONDS_PER_HOUR },

  /* E-Eur (Eastern Europe) [Nowhere actually uses this anymore lol] [Europe/Sofia]
  Rule  E-Eur 1981  max - Mar lastSun  0:00 1:00  S
  Rule  E-Eur 1996  max - Oct lastSun  0:00 0 -
  * Similarly to C-Eur, this is no longer used, but this is actually different from EU.
   Mar 28th 2015 22:00 UTC ~ Oct 25th 2015 21:00 UTC, GMT+2 */
  [12]={             1427580000,               1445720400, 2 * SECONDS_PER_HOUR },

  /* E-EurAsia (Georgia) [Nowhere actually uses this anymore lol] [Asia/Tbilisi]
  Rule E-EurAsia  1981  max - Mar lastSun  0:00 1:00  S
  Rule E-EurAsia  1996  max - Oct lastSun  0:00 0 -
  * Georgia gave up this time zone in 2005, and gave up DST entirely in 2006.
   Mar 28th 2015 20:00 UTC ~ Oct 24th 2015 19:00 UTC, GMT+4 */
  [13]={             1427572800,               1445713200, 4 * SECONDS_PER_HOUR },

  /* EU (Europe) [Europe/Tirane]
  Rule  EU  1981  max - Mar lastSun  1:00u  1:00  S
  Rule  EU  1996  max - Oct lastSun  1:00u  0 -
   Mar 29th 2015 01:00 UTC ~ Oct 25th 2015 01:00 UTC, GMT+1 */
  [14]={             1427590800,               1445734800, 1 * SECONDS_PER_HOUR },

  /* EUAsia (Europish Asia) [Asia/Nicosia]
  Rule  EUAsia  1981  max - Mar lastSun  1:00u  1:00  S
  Rule  EUAsia  1996  max - Oct lastSun  1:00u  0 -
  * This is literally the same as EU now.
   Mar 29th 2015 01:00 UTC ~ Oct 25th 2015 01:00 UTC, GMT+2 */
  [15]={             1427590800,               1445734800, 2 * SECONDS_PER_HOUR },

  /* Egypt (Egypt) [Africa/Cairo]
   * Egypt has abandoned DST */
  [16]={                      0,                        0, 2 * SECONDS_PER_HOUR },

  /* Fiji (Fiji Islands) [Pacific/Fiji]
  Rule  Fiji  2014  max - Nov Sun>=1  2:00  1:00  S
  Rule  Fiji  2015  max - Jan Sun>=18 3:00  0 -
   Nov  1st 2014 14:00 UTC ~ Jan 17th 2015 14:00 UTC, GMT+12 */
  [17]={             1414850400,               1421503200,12 * SECONDS_PER_HOUR },

  /* Haiti (Haiti) [America/Port-au-Prince]
   * Haiti has abandoned DST */
  [18]={                      0,                        0,-5 * SECONDS_PER_HOUR },

  /* Jordan (Jordan) [Asia/Amman]
  Rule  Jordan  2014  max - Mar lastThu 24:00 1:00  S
  Rule  Jordan  2014  max - Oct lastFri 0:00s 0 -
   Mar 26th 2015 22:00 UTC ~ Oct 29th 2015 22:00 UTC, GMT+2 */
  [19]={             1427407200,               1446156000, 2 * SECONDS_PER_HOUR },

  /* LH (Lord Howe Island) [Australia/Lord_Howe]
  Rule  LH  2008  max - Apr Sun>=1  2:00  0 S
  Rule  LH  2008  max - Oct Sun>=1  2:00  0:30  D
   Oct  4th 2014 15:30 UTC ~ Apr  4th 2015 15:00 UTC, GMT+10.5 */
  [20]={             1412436600,               1428159600,10.5 * SECONDS_PER_HOUR },

  /* Lebanon (Lebanon) [Asia/Beirut]
  Rule  Lebanon 1993  max - Mar lastSun 0:00  1:00  S
  Rule  Lebanon 1999  max - Oct lastSun 0:00  0 -
   Mar 28th 2015 22:00 UTC ~ Oct 24th 2015 21:00 UTC, GMT+2 */
  [21]={             1427580000,               1445720400, 2 * SECONDS_PER_HOUR },

  /* Mexico (Mexico) [America/Mexico_City]
  Rule  Mexico  2002  max - Apr Sun>=1  2:00  1:00  D
  Rule  Mexico  2002  max - Oct lastSun 2:00  0 S
   Apr  5th 2015 08:00 UTC ~ Oct 25th 2015 07:00 UTC, GMT-6 */
  [22]={             1428220800,               1445756400,-6 * SECONDS_PER_HOUR },

  /* Morocco (Morocco) [Africa/Casablanca]
  Rule  Azer  1997  max - Mar lastSun  4:00 1:00  S
  Rule  Azer  1997  max - Oct lastSun  5:00 0 -
  * TODO:
  * At least as insane as Egypt, without the possibility of parole.
   Mar 29th 2015 02:00 UTC ~ Oct 25th 2015 02:00 UTC, GMT+0 */
  [23]={             1427594400,               1445738400, 0 * SECONDS_PER_HOUR },

  /* NZ (New Zealand) [Pacific/Auckland]
  Rule  NZ  2007  max - Sep lastSun 2:00s 1:00  D
  Rule  NZ  2008  max - Apr Sun>=1  2:00s 0 S
   Sep 27th 2014 14:00 UTC ~ Apr  4th 2015 14:00 UTC, GMT+12 */
  [24]={             1411826400,               1428156000,12 * SECONDS_PER_HOUR },

  /* Namibia (Namibia) [Africa/Windhoek]
  Rule  Namibia 1994  max - Sep Sun>=1  2:00  1:00  S
  Rule  Namibia 1995  max - Apr Sun>=1  2:00  0 -
   Sep  7th 2014 01:00 UTC ~ Apr  5th 2015 00:00 UTC, GMT+1 */
  [25]={             1410051600,               1428192000, 1 * SECONDS_PER_HOUR },

  /* Palestine (Gaza/West Bank) [Asia/Gaza]
  Rule Palestine  2016    max -   Mar lastSat 1:00    1:00    S
  Rule Palestine  2016    max -   Oct lastSat 1:00    0   -
   Mar 27th 2015 23:00 UTC ~ Sep 24th 2015 21:00 UTC, GMT+2 */
  [26]={             1427497200,               1446242400, 2 * SECONDS_PER_HOUR },

  /* Para (Paraguay) [America/Asuncion]
  Rule  Para  2010  max - Oct Sun>=1  0:00  1:00  S
  Rule  Para  2013  max - Mar Sun>=22 0:00  0 -
   Oct  5th 2014 04:00 UTC ~ Mar 22nd 2015 03:00 UTC, GMT-4 */
  [27]={             1412481600,               1426993200,-4 * SECONDS_PER_HOUR },

  /* RussiaAsia (Some Asian Russian areas) [Nowhere uses this anymore] [Asia/Yerevan]
  Rule RussiaAsia 1993  max - Mar lastSun  2:00s  1:00  S
  Rule RussiaAsia 1996  max - Oct lastSun  2:00s  0 -
  * Armenia gave this up in 2012
   Mar 28th 2015 22:00 UTC ~ Oct 24th 2015 22:00 UTC, GMT+4 */
  [28]={                      0,                        0, 4 * SECONDS_PER_HOUR },

  /* Syria (Syria) [Asia/Damascus]
  Rule  Syria 2012  max - Mar lastFri 0:00  1:00  S
  Rule  Syria 2009  max - Oct lastFri 0:00  0 -
   Mar 26th 2015 22:00 UTC ~ Oct 29th 2015 21:00 UTC, GMT+2 */
  [29]={             1427407200,               1446152400, 2 * SECONDS_PER_HOUR },

  /* Thule (Thule Air Base) [America/Thule]
  Rule  Thule 2007  max - Mar Sun>=8  2:00  1:00  D
  Rule  Thule 2007  max - Nov Sun>=1  2:00  0 S
   Mar  8th 2015 06:00 UTC ~ Nov  1st 2015 05:00 UTC, GMT-4 */
  [30]={             1425794400,               1446354000,-4 * SECONDS_PER_HOUR },

  /* US (United States) [America/Los_Angeles]
  Rule  US  2007  max - Mar Sun>=8  2:00  1:00  D
  Rule  US  2007  max - Nov Sun>=1  2:00  0 S
   Mar  8th 2015 10:00 UTC ~ Nov  1st 2015 09:00 UTC, GMT-8 */
  [31]={             1425808800,               1446368400,-8 * SECONDS_PER_HOUR },

  /* Uruguay (Uruguay) [America/Montevideo]
   * Uruguay has abandoned DST */
  [32]={                      0,                        0,-3 * SECONDS_PER_HOUR },

  /* W-Eur (Western Europe) [Nowhere uses this anymore] [Europe/Lisbon]
  Rule  W-Eur 1981  max - Mar lastSun  1:00s  1:00  S
  Rule  W-Eur 1996  max - Oct lastSun  1:00s  0 -
  * Similarly to C-Eur, this is no longer used, but this is actually different from EU.
   Mar 29th 2015 00:00 UTC ~ Oct 25th 2015 01:00 UTC, GMT+0 */
  [33]={             1427590800,               1445734800, 0 * SECONDS_PER_HOUR },

  /* WS (Western Samoa) [Pacific/Apia]
  Rule  WS  2012  max - Apr Sun>=1  4:00  0 S
  Rule  WS  2012  max - Sep lastSun 3:00  1 D
   Sep 27th 2014 14:00 UTC ~ Apr  4th 2015 14:00 UTC, GMT+13 */
  [34]={             1411826400,               1428156000,13 * SECONDS_PER_HOUR },

  /* Zion (Israel) [Asia/Jerusalem]
  Rule  Zion  2013  max - Mar Fri>=23 2:00  1:00  D
  Rule  Zion  2013  max - Oct lastSun 2:00  0 S
   Mar 27th 2015 00:00 UTC ~ Oct 24th 2015 23:00 UTC, GMT+2 */
  [35]={             1427414400,               1445727600, 2 * SECONDS_PER_HOUR },
};

void prv_update_dstrule_timestamps_by_dstzone_id(TimezoneInfo *tz_info, time_t utc_time);

void test_clock__dstzone_rule_check(void) {
  s_prefs_24h_style = false;
  fake_rtc_init(0, 0);
  rtc_timezone_clear();
  clock_init();

  static const time_t jan1st_noon_2015 = 1420113600;

  for (int dstid = 0; dstid < DST_ID_COUNT; dstid++) {
    TimezoneInfo tz_info = {
      .dst_id = dstid,
      .tm_gmtoff = s_dst_correct_values[dstid][2]
    };

    prv_update_dstrule_timestamps_by_dstzone_id(&tz_info, jan1st_noon_2015);

    if (tz_info.dst_start != s_dst_correct_values[dstid][0]) {
      printf("start [%d] tz_info: %ld s_dst_correct_values: %ld\n",
             dstid, tz_info.dst_start, s_dst_correct_values[dstid][0]);
    }
    if (tz_info.dst_end != s_dst_correct_values[dstid][1]) {
      printf("  end [%d] tz_info: %ld s_dst_correct_values: %ld\n",
             dstid, tz_info.dst_end, s_dst_correct_values[dstid][1]);
    }

    cl_check(tz_info.dst_start == s_dst_correct_values[dstid][0]);
    cl_check(tz_info.dst_end == s_dst_correct_values[dstid][1]);
  }
}

void test_clock__next_monday(void) {
  struct tm jan_1 = {
    .tm_sec = 0, // 0 seconds after the minute
    .tm_min = 0, // 0 minutes after the hour
    .tm_hour = 0, // 0 hours since midnight
    .tm_mday = 1, // 1st day of the month
    .tm_mon = 0, // January
    .tm_year = 2014 - 1900,
    .tm_isdst = 0,
  };

  // next Monday (the 6th) at 17:30
  struct tm jan_6 = {
    .tm_sec = 0, // 0 seconds after the minute
    .tm_min = 30, // 30 minutes after the hour
    .tm_hour = 17, // 17 hours since midnight
    .tm_mday = 6, // 6st day of the month
    .tm_mon = 0, // January
    .tm_year = 2014 - 1900,
    .tm_isdst = 0,
  };

  // DST info for US/Canada 2014
  TimezoneInfo tz_info = {
    .dst_start = 1394330400, // Sun, 09 Mar 2014 02:00
    .dst_end = 1414893600// Sun, 02 Nov 2014 02:00
  };
  time_util_update_timezone(&tz_info);
  prv_set_current_time(jan_1);
  cl_assert_equal_i(clock_to_timestamp(MONDAY, 17, 30), mktime(&jan_6));
}

void test_clock__clock_to_timestamp(void) {
  s_prefs_24h_style = false;
  fake_rtc_init(0, 0);
  rtc_timezone_clear();

  static const time_t jan1st_noon_2005 = 1104580800;
  static const int32_t min_gmtoff = -12 * SECONDS_PER_HOUR;
  static const int32_t max_gmtoff =  12 * SECONDS_PER_HOUR;
  static const int32_t gmtoff_slide = SECONDS_PER_MINUTE;

  TimezoneInfo tzinfo = {{0}};
  tzinfo.dst_id = 0;
  tzinfo.dst_start = tzinfo.dst_end = 0;
  tzinfo.timezone_id = 0;
  for (int32_t gmtoff = min_gmtoff; gmtoff <= max_gmtoff; gmtoff += gmtoff_slide) {
    prv_clock_reset(0);

    tzinfo.tm_gmtoff = gmtoff;
    rtc_set_timezone(&tzinfo);
    clock_init();
    clock_set_time(jan1st_noon_2005);
    cl_assert_equal_i(rtc_get_time(), jan1st_noon_2005);

    time_t t = rtc_get_time();
    struct tm now;
    localtime_r(&t, &now);
    time_t timestamp = clock_to_timestamp(TODAY, now.tm_hour, now.tm_min + 1);

    cl_assert_equal_i(timestamp, t + 60);
  }
}

void test_clock__cross_dst(void) {
  struct tm oct_31 = {
    .tm_sec = 0,
    .tm_min = 59,
    .tm_hour = 23,
    .tm_mday = 31,
    .tm_mon = 9, // Oct
    .tm_year = 2015 - 1900,
    .tm_isdst = 1,
    .tm_gmtoff = -5 * SECONDS_PER_HOUR,
  };

  struct tm nov_7 = {
    .tm_sec = 0,
    .tm_min = 15,
    .tm_hour = 0,
    .tm_mday = 7,
    .tm_mon = 10, // Nov
    .tm_year = 2015 - 1900,
    .tm_isdst = 0, // Crossing daylight savings time barrier!
    .tm_gmtoff = -5 * SECONDS_PER_HOUR,
  };

  // DST info for US/Canada 2015
  TimezoneInfo tz_info = {
    .dst_start = 1425780000, // Sun, 08 Mar 2015 02:00
    .dst_end = 1446343200, // Sun, 01 Nov 2015 02:00
    .tm_gmtoff = -5 * SECONDS_PER_HOUR,
  };
  time_util_update_timezone(&tz_info);
  prv_set_current_time(oct_31);
  cl_assert_equal_i(clock_to_timestamp(SATURDAY, 0, 15), mktime(&nov_7));
}

void test_clock__today(void) {
  struct tm may_30 = {
    .tm_sec = 0,
    .tm_min = 59,
    .tm_hour = 7,
    .tm_mday = 30,
    .tm_mon = 4, // May
    .tm_year = 2016 - 1900,
    .tm_isdst = 1,
    .tm_gmtoff = -5 * SECONDS_PER_HOUR,
  };

  struct tm may_31 = {
    .tm_sec = 0,
    .tm_min = 0,
    .tm_hour = 5,
    .tm_mday = 31,
    .tm_mon = 4, // May
    .tm_year = 2016 - 1900,
    .tm_isdst = 1,
    .tm_gmtoff = -5 * SECONDS_PER_HOUR,
  };

  // DST info for US/Canada 2016
  TimezoneInfo tz_info = {
    .dst_id = 0,
    .dst_start = 1457834400, // Sun, 13 Mar 2016 02:00
    .dst_end = 1478397600, // Sun, 06 Nov 2016 02:00
    .tm_gmtoff = -5 * SECONDS_PER_HOUR,
  };
  time_util_update_timezone(&tz_info);
  prv_set_current_time(may_30);
  cl_assert_equal_i(clock_to_timestamp(TODAY, 5, 0), mktime(&may_31));
}

void test_clock__time_until_one_hour_relative(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 10th 2015, 14:00:00
  // Now + two hours
  const int event_time = jun10th_noon_2015 + (2 * SECONDS_PER_HOUR);

  // if the event is in 1+ hours, then show the actual time instead of "In X hours"
  const int MAX_RELATIVE_HRS = 1;

  // June 10th 2015, 12:00:00 (T-02:00:00)
  rtc_set_time(jun10th_noon_2015);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s(" 2:00 PM", time_buf);

  // June 8th 2015, 14:00:00 (T-48:00:00)
  rtc_set_time(event_time - SECONDS_PER_DAY - (24 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Wed,  2:00 PM", time_buf);

  // June 8th 2015, 23:59:58 (T-38:00:02)
  rtc_set_time(event_time - SECONDS_PER_DAY - (14 * SECONDS_PER_HOUR) - 2);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Wed,  2:00 PM", time_buf);

  // June 8th 2015, 23:59:59 (T-38:00:01)
  rtc_set_time(event_time - SECONDS_PER_DAY - (14 * SECONDS_PER_HOUR) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Wed,  2:00 PM", time_buf);

  // June 9th 2015, 14:00:00 (T-24:00:00)
  rtc_set_time(event_time - (24 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Wed,  2:00 PM", time_buf);

  // June 9th 2015, 23:59:58 (T-14:00:02)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR) - 2);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Wed,  2:00 PM", time_buf);

  // June 9th 2015, 23:59:59 (T-14:00:01)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Wed,  2:00 PM", time_buf);

  // June 10th 2015, 00:00:00 (T-14:00:00)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s(" 2:00 PM", time_buf);

  // June 10th 2015, 00:00:01 (T-13:59:59)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR) + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s(" 2:00 PM", time_buf);

  // June 10th 2015, 12:59:59 (T-01:00:01)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s(" 2:00 PM", time_buf);

  // June 10th 2015, 13:00:00 (T-01:00:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 H", time_buf);

  // June 10th 2015, 13:00:01 (T-00:59:59)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR) + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 H", time_buf);

  // June 10th 2015, 13:00:59 (T-00:59:01)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 H", time_buf);

  // June 10th 2015, 13:01:00 (T-00:59:00)
  rtc_set_time(event_time - (59 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 59 MIN", time_buf);

  // June 10th 2015, 13:01:59 (T-00:58:01)
  rtc_set_time(event_time - (58 * SECONDS_PER_MINUTE) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 59 MIN", time_buf);

  // June 10th 2015, 13:30:00 (T-00:30:00)
  rtc_set_time(event_time - (30 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 30 MIN", time_buf);

  // June 10th 2015, 13:30:29 (T-00:29:31)
  rtc_set_time(event_time - (30 * SECONDS_PER_MINUTE) + 29);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 30 MIN", time_buf);

  // June 10th 2015, 13:30:30 (T-00:29:30)
  rtc_set_time(event_time - (30 * SECONDS_PER_MINUTE) + 30);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 30 MIN", time_buf);

  // June 10th 2015, 13:30:59 (T-00:29:01)
  rtc_set_time(event_time - (30 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 30 MIN", time_buf);

  // June 10th 2015, 13:59:00 (T-00:01:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 MIN", time_buf);

  // June 10th 2015, 13:59:59 (T-00:00:01)
  rtc_set_time(event_time - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 MIN", time_buf);

  // June 10th 2015, 14:00:00 (T-00:00:00)
  rtc_set_time(event_time);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("NOW", time_buf);
}

void test_clock__time_until_twenty_four_hour_relative(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 12th 2015, 12:00
  // Now + two days
  const int event_time = jun10th_noon_2015 + (2 * SECONDS_PER_DAY);

  // if the event is in 24 hours on the same day, then show it.
  const int MAX_RELATIVE_HRS = 24;

  // June 10th 2015, 12:00:00 (T-48:00:00)
  rtc_set_time(jun10th_noon_2015);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 5th 2015, 12:00:00 (T-7DAY-00:00:00)
  rtc_set_time(jun10th_noon_2015 - (7 * SECONDS_PER_DAY));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Jun 12, 12:00 PM", time_buf);

  // June 5th 2015, 12:00:01 (T-7DAY+00:00:01)
  rtc_set_time(jun10th_noon_2015 - (7 * SECONDS_PER_DAY) + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Jun 12, 12:00 PM", time_buf);

  // June 10th 2015, 23:59:59 (T-2DAY+11:59:59)
  rtc_set_time(jun10th_noon_2015 + (12 * SECONDS_PER_HOUR) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 11th 2015, 00:00:00 (T-1DAY-12:00:00)
  rtc_set_time(event_time - SECONDS_PER_DAY - (12 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 11th 2015, 12:00:00 (T-24:00:00)
  rtc_set_time(event_time - SECONDS_PER_DAY);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 11th 2015, 12:00:01 (T-23:59:59)
  rtc_set_time(event_time - SECONDS_PER_DAY + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 11th 2015, 23:59:29 (T-12:00:31)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) - 31);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 11th 2015, 23:59:30 (T-12:00:30)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) - 30);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 11th 2015, 23:59:59 (T-12:00:01)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Fri, 12:00 PM", time_buf);

  // June 12th 2015, 00:00:00 (T-12:00:00)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 12 H", time_buf);

  // June 12th 2015, 00:00:01 (T-11:59:59)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 12 H", time_buf);

  // June 12th 2015, 00:29:29 (T-11:30:31)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) + (29 * SECONDS_PER_MINUTE) + 29);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 12 H", time_buf);

  // June 12th 2015, 00:29:30 (T-11:30:30)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) + (29 * SECONDS_PER_MINUTE) + 30);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 12 H", time_buf);

  // June 12th 2015, 00:29:59 (T-11:30:01)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) + (29 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 12 H", time_buf);

  // June 12th 2015, 00:30:00 (T-11:30:00)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 11 H", time_buf);

  // June 12th 2015, 00:30:01 (T-11:29:59)
  rtc_set_time(event_time - (12 * SECONDS_PER_HOUR) + (30 * SECONDS_PER_MINUTE) + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 11 H", time_buf);

  // June 12th 2015, 00:59:59 (T-11:00:01)
  rtc_set_time(event_time - (11 * SECONDS_PER_HOUR) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 11 H", time_buf);

  // June 12th 2015, 01:00:00 (T-11:00:00)
  rtc_set_time(event_time - (11 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 11 H", time_buf);

  // June 12th 2015, 01:00:01 (T-10:59:59)
  rtc_set_time(event_time - (11 * SECONDS_PER_HOUR) + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 11 H", time_buf);

  // June 12th 2015, 01:30:00 (T-10:30:00)
  rtc_set_time(event_time - (11 * SECONDS_PER_HOUR) + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 10 H", time_buf);
}

void test_clock__time_past_two_hour_relative(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 12:00:00
  // Now - one day
  const int event_time = jun10th_noon_2015 - SECONDS_PER_DAY;

  // if the event is within 2 hours, then show the actual time instead of "X hours ago"
  const int MAX_RELATIVE_HRS = 2;

  // June 9th 2015, 12:00:00 (T+00:00:00)
  rtc_set_time(event_time);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("NOW", time_buf);

  // June 9th 2015, 12:00:59 (T+00:00:59)
  rtc_set_time(event_time + (1 * SECONDS_PER_MINUTE) - 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("NOW", time_buf);

  // June 9th 2015, 12:01:00 (T+00:01:00)
  rtc_set_time(event_time + (1 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 MIN AGO", time_buf);

  // June 9th 2015, 12:05:00 (T+00:05:00)
  rtc_set_time(event_time + (5 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("5 MIN AGO", time_buf);

  // June 9th 2015, 12:10:00 (T+00:10:00)
  rtc_set_time(event_time + (10 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("10 MIN AGO", time_buf);

  // June 9th 2015, 12:10:00 (T+00:10:01)
  rtc_set_time(event_time + (10 * SECONDS_PER_MINUTE) + 1);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("10 MIN AGO", time_buf);

  // June 9th 2015, 12:10:30 (T+00:10:30)
  rtc_set_time(event_time + (10 * SECONDS_PER_MINUTE) + 30);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("10 MIN AGO", time_buf);

  // June 9th 2015, 12:10:59 (T+00:10:59)
  rtc_set_time(event_time + (10 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("10 MIN AGO", time_buf);

  // June 9th 2015, 12:59:29 (T+00:59:29)
  rtc_set_time(event_time + (59 * SECONDS_PER_MINUTE) + 29);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("59 MIN AGO", time_buf);

  // June 9th 2015, 12:59:30 (T+00:59:30)
  rtc_set_time(event_time + (59 * SECONDS_PER_MINUTE) + 30);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("59 MIN AGO", time_buf);

  // June 9th 2015, 12:59:58 (T+00:59:58)
  rtc_set_time(event_time + (59 * SECONDS_PER_MINUTE) + 58);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("59 MIN AGO", time_buf);

  // June 9th 2015, 12:59:59 (T+00:59:59)
  rtc_set_time(event_time + (59 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("59 MIN AGO", time_buf);

  // June 9th 2015, 13:00:00 (T+01:00:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 H AGO", time_buf);

  // June 9th 2015, 13:29:29 (T+01:29:29)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (29 * SECONDS_PER_MINUTE) + 29);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 H AGO", time_buf);

  // June 9th 2015, 13:29:30 (T+01:29:30)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (29 * SECONDS_PER_MINUTE) + 30);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 H AGO", time_buf);

  // June 9th 2015, 13:30:00 (T+01:30:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 H AGO", time_buf);

  // June 9th 2015, 13:59:59 (T+01:59:59)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (59 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 H AGO", time_buf);

  // June 9th 2015, 14:00:00 (T+02:00:00)
  rtc_set_time(event_time + (2 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("12:00 PM", time_buf);

  // June 9th 2015, 23:59:59 (T+11:59:59)
  rtc_set_time(event_time + (11 * SECONDS_PER_HOUR) + (59 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("12:00 PM", time_buf);

  // June 10th 2015, 00:00:00 (T+12:00:00)
  rtc_set_time(event_time + (12 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday, 12:00 PM", time_buf);

  // June 10th 2015, 11:00:00 (T+23:00:00)
  rtc_set_time(event_time + (23 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday, 12:00 PM", time_buf);

  // June 10th 2015, 13:00:00 (T+1DAY+01:00:00)
  rtc_set_time(event_time + SECONDS_PER_DAY + (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday, 12:00 PM", time_buf);

  // June 11th 2015, 13:00:00 (T+2DAY+01:00:00)
  rtc_set_time(event_time + (2 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Tue, 12:00 PM", time_buf);

  // June 16th 2015, 13:00:00 (T+7DAY+01:00:00)
  rtc_set_time(event_time + (7 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Jun  9, 12:00 PM", time_buf);
}

void test_clock__time_past_twenty_four_hour_relative(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 12:00:00
  // Now - one day
  const int event_time = jun10th_noon_2015 - SECONDS_PER_DAY;

  // if the event is within 24 hours, then show the actual time instead of "X hours ago"
  const int MAX_RELATIVE_HRS = 24;

  // June 9th 2015, 13:30:00 (T+01:30:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 H AGO", time_buf);

  // June 9th 2015, 13:59:59 (T+01:59:59)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (59 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 H AGO", time_buf);

  // June 9th 2015, 14:00:00 (T+02:00:00)
  rtc_set_time(event_time + (2 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 H AGO", time_buf);

  // June 9th 2015, 23:59:59 (T+11:59:59)
  rtc_set_time(event_time + (11 * SECONDS_PER_HOUR) + (59 * SECONDS_PER_MINUTE) + 59);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("12 H AGO", time_buf);

  // June 10th 2015, 00:00:00 (T+12:00:00)
  rtc_set_time(event_time + (12 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday, 12:00 PM", time_buf);

  // June 10th 2015, 11:00:00 (T+23:00:00)
  rtc_set_time(event_time + (23 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday, 12:00 PM", time_buf);
}

void test_clock__time_12h_style(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 16:00:00
  // Now - one day
  const int event_time = jun10th_noon_2015 - SECONDS_PER_DAY + (4 * SECONDS_PER_HOUR);

  // if the event is within 24 hours, then show the actual time instead of "X hours ago"
  const int MAX_RELATIVE_HRS = 13;

  clock_set_24h_style(false);
  // June 9th 2015, 17:00:00 (T+01:00:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 H AGO", time_buf);

  // June 9th 2015, 17:30:00 (T+01:30:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 H AGO", time_buf);

  // June 9th 2015, 16:01:00 (T+00:01:00)
  rtc_set_time(event_time + (1 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 MIN AGO", time_buf);

  // June 9th 2015, 16:02:00 (T+00:02:00)
  rtc_set_time(event_time + (2 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 MIN AGO", time_buf);

  // June 9th 2015, 15:00:00 (T-01:00:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 H", time_buf);

  // June 9th 2015, 14:00:00 (T-02:00:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 2 H", time_buf);

  // June 9th 2015, 15:59:00 (T-00:01:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 MIN", time_buf);

  // June 9th 2015, 15:58:00 (T-00:02:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 2 MIN", time_buf);

  // June 10th 2015, 04:00:00 (T+12:00:00)
  rtc_set_time(event_time + (12 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday,  4:00 PM", time_buf);

  // June 9th 2015, 02:00:00 (T-14:00:00)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s(" 4:00 PM", time_buf);

  // June 8th 2015, 16:00:00 (T-48:00:00)
  rtc_set_time(event_time - SECONDS_PER_DAY - (24 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Tue,  4:00 PM", time_buf);

  // June 16th 2015, 17:00:00 (T+7DAY+01:00:00)
  rtc_set_time(event_time + (7 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Jun  9,  4:00 PM", time_buf);
}

void test_clock__time_24h_style(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 16:00:00
  // Now - one day
  const int event_time = jun10th_noon_2015 - SECONDS_PER_DAY + (4 * SECONDS_PER_HOUR);

  // if the event is within 24 hours, then show the actual time instead of "X hours ago"
  const int MAX_RELATIVE_HRS = 13;

  clock_set_24h_style(true);
  // June 9th 2015, 17:00:00 (T+01:00:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR);
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 H AGO", time_buf);

  // June 9th 2015, 17:30:00 (T+01:30:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 H AGO", time_buf);

  // June 9th 2015, 16:01:00 (T+00:01:00)
  rtc_set_time(event_time + (1 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 MIN AGO", time_buf);

  // June 9th 2015, 16:02:00 (T+00:02:00)
  rtc_set_time(event_time + (2 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 MIN AGO", time_buf);

  // June 9th 2015, 15:00:00 (T-01:00:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 H", time_buf);

  // June 9th 2015, 14:00:00 (T-02:00:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 2 H", time_buf);

  // June 9th 2015, 15:59:00 (T-00:01:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 1 MIN", time_buf);

  // June 9th 2015, 15:58:00 (T-00:02:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_MINUTE));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("IN 2 MIN", time_buf);

  // June 10th 2015, 04:00:00 (T+12:00:00)
  rtc_set_time(event_time + (12 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday, 16:00", time_buf);

  // June 9th 2015, 02:00:00 (T-14:00:00)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("16:00", time_buf);

  // June 8th 2015, 16:00:00 (T-48:00:00)
  rtc_set_time(event_time - SECONDS_PER_DAY - (24 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Tue, 16:00", time_buf);

  // June 16th 2015, 17:00:00 (T+7DAY+01:00:00)
  rtc_set_time(event_time + (7 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR));
  clock_get_until_time_capitalized(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Jun  9, 16:00", time_buf);
}

void test_clock__time_12h_lower_style(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 16:00:00
  // Now - one day
  const int event_time = jun10th_noon_2015 - SECONDS_PER_DAY + (4 * SECONDS_PER_HOUR);

  // if the event is within 24 hours, then show the actual time instead of "X hours ago"
  const int MAX_RELATIVE_HRS = 13;

  clock_set_24h_style(false);
  // June 9th 2015, 17:00:00 (T+01:00:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR);
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("An hour ago", time_buf);

  // June 9th 2015, 17:30:00 (T+01:30:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 hours ago", time_buf);

  // June 9th 2015, 16:01:00 (T+00:01:00)
  rtc_set_time(event_time + (1 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 minute ago", time_buf);

  // June 9th 2015, 16:02:00 (T+00:02:00)
  rtc_set_time(event_time + (2 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 minutes ago", time_buf);

  // June 9th 2015, 15:00:00 (T-01:00:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 1 hour", time_buf);

  // June 9th 2015, 14:00:00 (T-02:00:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 2 hours", time_buf);

  // June 9th 2015, 15:59:00 (T-00:01:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 1 minute", time_buf);

  // June 9th 2015, 15:58:00 (T-00:02:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 2 minutes", time_buf);

  // June 10th 2015, 04:00:00 (T+12:00:00)
  rtc_set_time(event_time + (12 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday,  4:00 PM", time_buf);

  // June 9th 2015, 02:00:00 (T-14:00:00)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s(" 4:00 PM", time_buf);

  // June 8th 2015, 16:00:00 (T-48:00:00)
  rtc_set_time(event_time - SECONDS_PER_DAY - (24 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Tue,  4:00 PM", time_buf);

  // June 16th 2015, 17:00:00 (T+7DAY+01:00:00)
  rtc_set_time(event_time + (7 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Jun  9,  4:00 PM", time_buf);
}

void test_clock__time_24h_lower_style(void) {
  char time_buf[64];

  const int jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 16:00:00
  // Now - one day
  const int event_time = jun10th_noon_2015 - SECONDS_PER_DAY + (4 * SECONDS_PER_HOUR);

  // if the event is within 24 hours, then show the actual time instead of "X hours ago"
  const int MAX_RELATIVE_HRS = 13;

  clock_set_24h_style(true);
  // June 9th 2015, 17:00:00 (T+01:00:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR);
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("An hour ago", time_buf);

  // June 9th 2015, 17:30:00 (T+01:30:00)
  rtc_set_time(event_time + SECONDS_PER_HOUR + (30 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 hours ago", time_buf);

  // June 9th 2015, 16:01:00 (T+00:01:00)
  rtc_set_time(event_time + (1 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("1 minute ago", time_buf);

  // June 9th 2015, 16:02:00 (T+00:02:00)
  rtc_set_time(event_time + (2 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("2 minutes ago", time_buf);

  // June 9th 2015, 15:00:00 (T-01:00:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 1 hour", time_buf);

  // June 9th 2015, 14:00:00 (T-02:00:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 2 hours", time_buf);

  // June 9th 2015, 15:59:00 (T-00:01:00)
  rtc_set_time(event_time - (1 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 1 minute", time_buf);

  // June 9th 2015, 15:58:00 (T-00:02:00)
  rtc_set_time(event_time - (2 * SECONDS_PER_MINUTE));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("In 2 minutes", time_buf);

  // June 10th 2015, 04:00:00 (T+12:00:00)
  rtc_set_time(event_time + (12 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Yesterday, 16:00", time_buf);

  // June 9th 2015, 02:00:00 (T-14:00:00)
  rtc_set_time(event_time - (14 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("16:00", time_buf);

  // June 8th 2015, 16:00:00 (T-48:00:00)
  rtc_set_time(event_time - SECONDS_PER_DAY - (24 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Tue, 16:00", time_buf);

  // June 16th 2015, 17:00:00 (T+7DAY+01:00:00)
  rtc_set_time(event_time + (7 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR));
  clock_get_until_time(time_buf, sizeof(time_buf), event_time, MAX_RELATIVE_HRS);
  cl_assert_equal_s("Jun  9, 16:00", time_buf);
}


void test_clock__month_named_date(void) {
  char time_buf[64];

  const time_t jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 16:00:00
  // Now - one day
  const time_t event_time = jun10th_noon_2015 - SECONDS_PER_DAY + (4 * SECONDS_PER_HOUR);
  time_t format_time;

  clock_set_24h_style(true);
  // June 9th 2015, 12:00:00 (-07:00:00)
  format_time = event_time + SECONDS_PER_HOUR;
  clock_get_month_named_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("June 9", time_buf);

  // June 10th 2015, 12:00:00 (-07:00:00)
  format_time = event_time + (24 * SECONDS_PER_HOUR);
  clock_get_month_named_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("June 10", time_buf);

  // June 7th 2015, 12:00:00 (-07:00:00)
  format_time = event_time - SECONDS_PER_DAY - (24 * SECONDS_PER_HOUR);
  clock_get_month_named_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("June 7", time_buf);

  // June 16th 2015, 13:00:00 (-07:00:00)
  format_time = event_time + (7 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR);
  clock_get_month_named_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("June 16", time_buf);
}

void test_clock__month_named_abbrev_date(void) {
  char time_buf[64];

  const time_t jun10th_noon_2015 = 1433937600;

  prv_clock_reset(0);
  rtc_set_time(jun10th_noon_2015);

  // Our test event is at June 9th 2015, 16:00:00
  // Now - one day
  const time_t event_time = jun10th_noon_2015 - SECONDS_PER_DAY + (4 * SECONDS_PER_HOUR);
  time_t format_time;

  clock_set_24h_style(true);
  // June 9th 2015, 12:00:00 (-07:00:00)
  format_time = event_time + SECONDS_PER_HOUR;
  clock_get_month_named_abbrev_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("Jun 9", time_buf);

  // June 10th 2015, 12:00:00 (-07:00:00)
  format_time = event_time + (24 * SECONDS_PER_HOUR);
  clock_get_month_named_abbrev_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("Jun 10", time_buf);

  // June 7th 2015, 12:00:00 (-07:00:00)
  format_time = event_time - SECONDS_PER_DAY - (24 * SECONDS_PER_HOUR);
  clock_get_month_named_abbrev_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("Jun 7", time_buf);

  // June 16th 2015, 13:00:00 (-07:00:00)
  format_time = event_time + (7 * SECONDS_PER_DAY) + (1 * SECONDS_PER_HOUR);
  clock_get_month_named_abbrev_date(time_buf, sizeof(time_buf), format_time);
  cl_assert_equal_s("Jun 16", time_buf);
}

void test_clock__relative_daypart_string(void) {
  const char *daypart_string = NULL;

  const char morning[] = "this morning";  // anything before 12pm of the current day
  const char afternoon[] = "this afternoon";  // 12pm today
  const char evening[] = "this evening";  // 6pm today
  const char tonight[] = "tonight"; // 9pm today
  const char tomorrow_morning[] = "tomorrow morning";  // 9am tomorrow
  const char tomorrow_afternoon[] = "tomorrow afternoon";  // 12pm tomorrow
  const char tomorrow_evening[] = "tomorrow evening";  // 6pm tomorrow
  const char tomorrow_night[] = "tomorrow night";  // 9pm tomorrow
  // starting 9am 2 days from now and ends midnight 2 days from now
  const char day_after_tomorrow[] = "the day after tomorrow";
  const char future[] = "the foreseeable future";  // Catchall for beyond 3 days

  // Our test event is at Feb 24 2015, 4:59:00 AM (Day of Second kickstarter)
  const int feb24_2015 = 1424753940;

  prv_clock_reset(0);
  rtc_set_time(feb24_2015);

  time_t timestamp = rtc_get_time();

  // The following are for "Powered 'til"
  // Which is read as "Powered 'til at least" ...

  // (4am today) Any time before 12pm is this morning
  daypart_string = clock_get_relative_daypart_string(timestamp, 0 /* hours_in_the_future */);
  cl_assert_equal_s(morning, daypart_string);

  // (8am today) time before 12pm is this morning
  daypart_string = clock_get_relative_daypart_string(timestamp, 4 /* hours_in_the_future */);
  cl_assert_equal_s(morning, daypart_string);

  // (9am today) time before 12pm is this morning
  daypart_string = clock_get_relative_daypart_string(timestamp, 5 /* hours_in_the_future */);
  cl_assert_equal_s(morning, daypart_string);

  // (4pm today) time between 12pm and 6pm is this afternoon
  daypart_string = clock_get_relative_daypart_string(timestamp, 12 /* hours_in_the_future */);
  cl_assert_equal_s(afternoon, daypart_string);

  // (8pm today) time between 6pm and 9pm is this evening
  daypart_string = clock_get_relative_daypart_string(timestamp, 16 /* hours_in_the_future */);
  cl_assert_equal_s(evening, daypart_string);

  // (9pm today) time between 9pm and tomorrow 9am is tonight
  daypart_string = clock_get_relative_daypart_string(timestamp, 17 /* hours_in_the_future */);
  cl_assert_equal_s(tonight, daypart_string);

  // (8am tomorrow) time between 9pm and tomorrow 9am is tonight
  daypart_string = clock_get_relative_daypart_string(timestamp, 28 /* hours_in_the_future */);
  cl_assert_equal_s(tonight, daypart_string);

  // (9am tomorrow) time tomorrow between 9am and 12pm is tomorrow morning
  daypart_string = clock_get_relative_daypart_string(timestamp, 29 /* hours_in_the_future */);
  cl_assert_equal_s(tomorrow_morning, daypart_string);

  // (12pm tomorrow) time tomorrow between 12pm and 6pm is tomorrow afternoon
  daypart_string = clock_get_relative_daypart_string(timestamp, 32 /* hours_in_the_future */);
  cl_assert_equal_s(tomorrow_afternoon, daypart_string);

  // (6pm tomorrow) time tomorrow between 6pm and 9pm is tomorrow evening
  daypart_string = clock_get_relative_daypart_string(timestamp, 38 /* hours_in_the_future */);
  cl_assert_equal_s(tomorrow_evening, daypart_string);

  // (9pm tomorrow) time tomorrow between 9pm and 9am the next day is tomorrow night
  daypart_string = clock_get_relative_daypart_string(timestamp, 41 /* hours_in_the_future */);
  cl_assert_equal_s(tomorrow_night, daypart_string);

  // (9am 2 days from now) time between 9am and 9pm 2 days from now
  daypart_string = clock_get_relative_daypart_string(timestamp, 53 /* hours_in_the_future */);
  cl_assert_equal_s(day_after_tomorrow, daypart_string);

  // (11pm 2 days from now) time between 2 days from now 9am and midnight is the day after tomorrow
  daypart_string = clock_get_relative_daypart_string(timestamp, 67 /* hours_in_the_future */);
  cl_assert_equal_s(day_after_tomorrow, daypart_string);

  // (midnight 2 days from now) time between 2 days from now 9am and midnight
  // is the day after tomorrow
  daypart_string = clock_get_relative_daypart_string(timestamp, 68 /* hours_in_the_future */);
  cl_assert_equal_s(day_after_tomorrow, daypart_string);

  // (1am 3 days from now) Anything after 2 days from now becomes "the foreseeable future"
  daypart_string = clock_get_relative_daypart_string(timestamp, 69 /* hours_in_the_future */);
  cl_assert_equal_s(future, daypart_string);

  // Our test event is at Oct 31 2015, 22:00:00
  const int oct31_2015 = 1446328800;

  prv_clock_reset(0);
  rtc_set_time(oct31_2015);

  timestamp = rtc_get_time();

  // (10pm today) time between 9pm and tomorrow 9am is tonight
  daypart_string = clock_get_relative_daypart_string(timestamp, 0 /* hours_in_the_future */);
  cl_assert_equal_s(tonight, daypart_string);

  // (9am tomorrow) time between 9pm and tomorrow 9am is tomorrow morning
  daypart_string = clock_get_relative_daypart_string(timestamp, 11 /* hours_in_the_future */);
  cl_assert_equal_s(tomorrow_morning, daypart_string);

  // Our test event is at Jan 1st 2016, 21:00:00
  const int jan1_2016 = 1451682000;

  prv_clock_reset(0);
  rtc_set_time(jan1_2016);

  timestamp = rtc_get_time();

  // (9pm today) time between 9pm and tomorrow 9am is tonight
  daypart_string = clock_get_relative_daypart_string(timestamp, 0 /* hours_in_the_future */);
  cl_assert_equal_s(tonight, daypart_string);
}

void test_clock__hour_and_minute_add(void) {
  int hour = 10;
  int minute = 15;
  clock_hour_and_minute_add(&hour, &minute, -30);
  cl_assert_equal_i(hour, 9);
  cl_assert_equal_i(minute, 45);

  hour = 23;
  minute = 15;
  clock_hour_and_minute_add(&hour, &minute, 65);
  cl_assert_equal_i(hour, 0);
  cl_assert_equal_i(minute, 20);

  hour = 0;
  minute = 15;
  clock_hour_and_minute_add(&hour, &minute, -30);
  cl_assert_equal_i(hour, 23);
  cl_assert_equal_i(minute, 45);
}
