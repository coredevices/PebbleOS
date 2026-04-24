/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

typedef enum HRZone {
  HRZone_Zone0,
  HRZone_Zone1,
  HRZone_Zone2,
  HRZone_Zone3,

  HRZoneCount,
  HRZone_Max = HRZone_Zone3,
} HRZone;

//! Returns the HR Zone for a given BPM
HRZone hr_util_get_hr_zone(int bpm);

//! Returns whether the BPM should be considered elevated
bool hr_util_is_elevated(int bpm);
