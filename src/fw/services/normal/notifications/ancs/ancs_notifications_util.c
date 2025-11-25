/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ancs_notifications_util.h"

#include "drivers/rtc.h"
#include "resource/timeline_resource_ids.auto.h"
#include "system/passert.h"
#include "util/attributes.h"
#include "util/date.h"
#include "util/pstring.h"
#include "util/size.h"
#include "util/string.h"

const ANCSAppMetadata* ancs_notifications_util_get_app_metadata(const ANCSAttribute *app_id) {
  static const ANCSAppMetadata s_generic_app = {
#if PBL_COLOR
    .app_color = GColorClearARGB8,
#endif
    .icon_id = TIMELINE_RESOURCE_NOTIFICATION_GENERIC,
  };

  static const struct ANCSAppMetadata map[] = {
#include "ancs_known_apps.h"
  };

  for (unsigned int index = 0; index < ARRAY_LENGTH(map); ++index) {
    const struct ANCSAppMetadata *mapping = &map[index];
    if (pstring_equal_cstring(&app_id->pstr, mapping->app_id)) {
      return mapping;
    }
  }

  // App ID doesn't match any of the known IDs:
  return &s_generic_app;
}

time_t ancs_notifications_util_parse_timestamp(const ANCSAttribute *timestamp_attr) {
  PBL_ASSERTN(timestamp_attr);
  struct PACKED {
    char year[4];
    char month[2];
    char day[2];
    char T[1];
    char hour[2];
    char minute[2];
    char second[2];
    char Z[1];
  } timestamp;

  // Make sure the attribute is the length we expect and that it doesn't have random NULL
  // characters in the middle
  if (timestamp_attr->length < sizeof(timestamp) - 1 ||
      strnlen((char *)timestamp_attr->value, sizeof(timestamp) - 1) < sizeof(timestamp) - 1) {
    // invalid length
    return 0;
  }

  memcpy(&timestamp, timestamp_attr->value, sizeof(timestamp) - 1);
  timestamp.Z[0] = '\0';

  if (timestamp.year[0] != '2' || timestamp.year[1] != '0') {
    // invalid data, we have bigger fishes to fry than the year 2100 -FBO
    return 0;
  }

  struct tm time_tm = { 0 };
  time_tm.tm_sec = atoi(timestamp.second);
  timestamp.second[0] = '\0';
  time_tm.tm_min = atoi(timestamp.minute);
  timestamp.minute[0] = '\0';
  time_tm.tm_hour = atoi(timestamp.hour);
  timestamp.T[0] = '\0';
  time_tm.tm_mday = atoi(timestamp.day);
  timestamp.day[0] = '\0';
  time_tm.tm_mon = atoi(timestamp.month) - 1;
  timestamp.month[0] = '\0';
  time_tm.tm_year = atoi(timestamp.year) - STDTIME_YEAR_OFFSET;

  // We have to assume that the timezone of the phone matches the timezone of the watch
  time_t sys_time = rtc_get_time();
  time_tm.tm_gmtoff = time_get_gmtoffset();
  time_get_timezone_abbr(time_tm.tm_zone, sys_time);
  time_tm.tm_isdst = time_get_isdst(sys_time);

  return mktime(&time_tm);
}

bool ancs_notifications_util_is_phone(const ANCSAttribute *app_id) {
  return (app_id && pstring_equal_cstring(&app_id->pstr, IOS_PHONE_APP_ID));
}

bool ancs_notifications_util_is_sms(const ANCSAttribute *app_id) {
  return (app_id && pstring_equal_cstring(&app_id->pstr, IOS_SMS_APP_ID));
}

bool ancs_notifications_util_is_group_sms(const ANCSAttribute *app_id,
                                          const ANCSAttribute *subtitle) {
  if (!ancs_notifications_util_is_sms(app_id)) {
    return false;
  }

  // The defining feature of a group sms (vs a regular sms) is that it has a subtitle field
  if (!subtitle || subtitle->length == 0) {
    return false;
  }

  return true;
}
