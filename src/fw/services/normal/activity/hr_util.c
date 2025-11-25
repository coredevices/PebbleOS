/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hr_util.h"

#include "activity.h"

// ------------------------------------------------------------------------------------------------
HRZone hr_util_get_hr_zone(int bpm) {
  const int zone_thresholds[HRZone_Max] = {
    activity_prefs_heart_get_zone1_threshold(),
    activity_prefs_heart_get_zone2_threshold(),
    activity_prefs_heart_get_zone3_threshold(),
  };

  HRZone zone;
  for (zone = HRZone_Zone0; zone < HRZone_Max; zone++) {
    if (bpm < zone_thresholds[zone]) {
      break;
    }
  }
  return zone;
}

bool hr_util_is_elevated(int bpm) {
  return bpm >= activity_prefs_heart_get_elevated_hr();
}
