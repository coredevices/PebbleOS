/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "data.h"
#include "hr_chart.h"

#include <applib/ui/ui.h>

//! Creates a health hr detail window
//! @param health_data pointer to the health data to be given to this card
//! @param today today's heart rate for the chart, not needed after this returns
//! @return A pointer to a newly allocated health hr detail window
Window *health_hr_detail_card_create(HealthData *health_data, const HealthHrChartData *today);

//! Destroys a health hr detail window
//! @param window Window pointer to health hr detail window
void health_hr_detail_card_destroy(Window *window);
