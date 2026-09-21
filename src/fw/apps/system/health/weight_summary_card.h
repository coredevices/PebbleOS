/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "data.h"

#include "applib/graphics/gtypes.h"
#include "applib/ui/layer.h"

Layer *health_weight_summary_card_create(HealthData *health_data);
void health_weight_summary_card_select_click_handler(Layer *layer);
void health_weight_summary_card_destroy(Layer *layer);
GColor health_weight_summary_card_get_bg_color(Layer *layer);
bool health_weight_summary_show_select_indicator(Layer *layer);
