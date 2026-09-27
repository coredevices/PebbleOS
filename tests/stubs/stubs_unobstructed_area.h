/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/unobstructed_area_service_private.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK unobstructed_area_service_get_area(UnobstructedAreaState *state, GRect *area) {
}

bool PBL_WEAK unobstructed_area_service_has_requested_area(UnobstructedAreaState *state) {
  return false;
}

void PBL_WEAK unobstructed_area_service_will_change(int16_t current_y, int16_t final_y) {
}

void PBL_WEAK unobstructed_area_service_change(int16_t current_y, int16_t final_y,
                                               AnimationProgress progress) {
}

void PBL_WEAK unobstructed_area_service_did_change(int16_t final_y) {
}
