/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "health_data.h"

#include "applib/ui/ui.h"

//! Creates a health activity detail window
//! @param HealthData pointer to the health data to be given to this card
//! @return A pointer to a newly allocated health activity detail window
Window *health_activity_detail_card_create(HealthData *health_data);

//! Destroys a health activity detail window
//! @param window Window pointer to health activity detail window
void health_activity_detail_card_destroy(Window *window);
