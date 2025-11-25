/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/activity/hr_util.h"

HRZone hr_util_get_hr_zone(int bpm) {
  return HRZone_Zone1;
}
