/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/unobstructed_area_service_private.h"
#include "util/attributes.h"

void WEAK unobstructed_area_service_get_area(UnobstructedAreaState *state, GRect *area) { }

void WEAK unobstructed_area_service_will_change(int16_t current_y, int16_t final_y) { }

void WEAK unobstructed_area_service_change(int16_t current_y, int16_t final_y,
                                           AnimationProgress progress) { }

void WEAK unobstructed_area_service_did_change(int16_t final_y) { }
