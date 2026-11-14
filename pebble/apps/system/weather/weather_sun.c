/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weather_sun.h"

#include "pebble_compat.h"

#include <time.h>

#include <pbl/kernel/time.h>

// The standard sunrise/sunset algorithm in fixed point, using Pebble's integer
// trig (sin_lookup/cos_lookup; acos via binary search).

static int32_t prv_norm_angle(int32_t a) {
  a %= TRIG_MAX_ANGLE;
  if (a < 0)
    a += TRIG_MAX_ANGLE;
  return a;
}
static int32_t prv_angle_from_cdeg(int32_t cdeg) { // degrees*100 -> trig angle
  return prv_norm_angle((int32_t)(((int64_t)cdeg * TRIG_MAX_ANGLE) / 36000));
}

// -0.83 deg: refraction plus solar radius
#define SUNSET_ELEV_CDEG (-83)
// The evening golden hour starts with the sun 6 degrees above the horizon
#define GOLDEN_ELEV_CDEG (600)

// Solar noon and the time from it to the sun standing at elev_cdeg (degrees * 100), both local
// minutes * 1000, plus the location's local time now in minutes. False if the location is
// unknown, or if strict and the sun does not cross that elevation today.
static bool prv_solar_day(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min, int elev_cdeg,
                          bool strict, int *noon_m1000, int *half_m1000, int *now_min) {
  if (lat_e2 == INT16_MIN || lon_e2 == INT16_MIN) {
    return false;
  }
  time_t now = pbl_realtime();
  struct pbl_tm *ltp = localtime(&now);
  if (!ltp) {
    return false;
  }
  const struct pbl_tm lt = *ltp;
  const int N = lt.tm_yday + 1; // day of year 1..366
  // The LOCATION's UTC offset when the phone synced one (v4.1) — so a saved city's
  // sun times read in that city's local clock; otherwise the watch's own offset.
  // tm_gmtoff, not time_get_gmtoffset(): the latter excludes DST while lt does not.
  const int watch_tz_min = (int)(lt.tm_gmtoff / 60);
  const int tz_min = (utc_off_min != INT16_MIN) ? utc_off_min : watch_tz_min;

  const int b_cdeg = (36000 * (N - 81)) / 365; // B = (360/365)*(N-81), deg*100
  const int32_t sinB = sin_lookup(prv_angle_from_cdeg(b_cdeg));
  const int32_t cosB = cos_lookup(prv_angle_from_cdeg(b_cdeg));
  const int32_t sin2B = sin_lookup(prv_angle_from_cdeg(2 * b_cdeg));

  const int decl_cdeg = (int)(((int64_t)2345 * sinB) / TRIG_MAX_RATIO); // 23.45*sin(B)
  const int32_t sinDec = sin_lookup(prv_angle_from_cdeg(decl_cdeg));
  const int32_t cosDec = cos_lookup(prv_angle_from_cdeg(decl_cdeg));
  const int32_t sinLat = sin_lookup(prv_angle_from_cdeg(lat_e2));
  const int32_t cosLat = cos_lookup(prv_angle_from_cdeg(lat_e2));

  const int64_t den = (int64_t)cosLat * cosDec;
  if (den == 0)
    return false;
  const int32_t sinH0 = sin_lookup(prv_angle_from_cdeg(elev_cdeg));
  int64_t cosH =
      ((int64_t)sinH0 * TRIG_MAX_RATIO - (int64_t)sinLat * sinDec) * 1000000 / den; // ratio *1e6
  if (strict && (cosH > 1000000 || cosH < -1000000))
    return false;
  // Clamped: polar night gives omega 0 (no day), polar day omega 180 (no night).
  if (cosH > 1000000)
    cosH = 1000000;
  if (cosH < -1000000)
    cosH = -1000000;

  int32_t alo = 0, ahi = TRIG_MAX_ANGLE / 2; // omega = acos(cosH), binary search
  for (int i = 0; i < 22; i++) {
    int32_t amid = (alo + ahi) / 2;
    int64_t c = (int64_t)cos_lookup(amid) * 1000000 / TRIG_MAX_RATIO;
    if (c > cosH)
      alo = amid;
    else
      ahi = amid;
  }
  const int omega_cdeg = (int)(((int64_t)((alo + ahi) / 2) * 36000) / TRIG_MAX_ANGLE);

  const int eot_m1000 =
      (int)(((int64_t)9870 * sin2B - (int64_t)7530 * cosB - (int64_t)1500 * sinB) / TRIG_MAX_RATIO);
  const int tc_m1000 = 40 * (int)lon_e2 - tz_min * 1000 + eot_m1000;
  *noon_m1000 = 720000 - tc_m1000;
  *half_m1000 = 40 * omega_cdeg; // 4 minutes per degree of hour angle
  const int mins = lt.tm_hour * 60 + lt.tm_min - watch_tz_min + tz_min;
  *now_min = ((mins % 1440) + 1440) % 1440;
  return true;
}

// The evening minute the sun stands at elev_cdeg, [0,1440), or -1
static int prv_evening_minutes(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min, int elev_cdeg,
                               bool strict) {
  int noon_m1000, half_m1000, now_min;
  if (!prv_solar_day(lat_e2, lon_e2, utc_off_min, elev_cdeg, strict, &noon_m1000, &half_m1000,
                     &now_min)) {
    return -1;
  }
  int minute = (noon_m1000 + half_m1000) / 1000;
  minute %= 1440;
  if (minute < 0)
    minute += 1440;
  return minute;
}

int weather_sun_sunset_minutes(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min) {
  return prv_evening_minutes(lat_e2, lon_e2, utc_off_min, SUNSET_ELEV_CDEG, false);
}

int weather_sun_golden_minutes(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min) {
  return prv_evening_minutes(lat_e2, lon_e2, utc_off_min, GOLDEN_ELEV_CDEG, true);
}

bool weather_sun_is_down(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min) {
  int noon_m1000, half_m1000, now_min;
  if (!prv_solar_day(lat_e2, lon_e2, utc_off_min, SUNSET_ELEV_CDEG, false, &noon_m1000, &half_m1000,
                     &now_min)) {
    return false;
  }
  // Minutes from solar noon, wrapped to [-720, 720).
  int from_noon = (now_min * 1000 - noon_m1000) / 1000;
  from_noon = (((from_noon + 720) % 1440) + 1440) % 1440 - 720;
  return (from_noon < 0 ? -from_noon : from_noon) * 1000 > half_m1000;
}
