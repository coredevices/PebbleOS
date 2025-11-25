/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "health_data.h"

#include "applib/ui/ui.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// API Functions
//

//! Creates a special layer with data
//! @param health_data A pointer to the health data being given this card
//! @return A pointer to a newly allocated layer, which contains its own data
Layer *health_activity_summary_card_create(HealthData *health_data);

//! Health activity summary select click handler
//! @param layer A pointer to an existing layer containing its own data
void health_activity_summary_card_select_click_handler(Layer *layer);

//! Destroy a special layer
//! @param base_layer A pointer to an existing layer containing its own data
void health_activity_summary_card_destroy(Layer *base_layer);

//! Health activity summary layer background color getter
GColor health_activity_summary_card_get_bg_color(Layer *layer);

//! Health activity summary layer select click is available
bool health_activity_summary_show_select_indicator(Layer *layer);
