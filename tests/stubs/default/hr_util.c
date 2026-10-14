/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/activity/hr_util.h>

PBL_WEAK HRZone hr_util_get_hr_zone(int bpm) {
  return HRZone_Zone1;
}
