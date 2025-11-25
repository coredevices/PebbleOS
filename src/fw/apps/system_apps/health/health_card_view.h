/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "health_data.h"

#include "applib/ui/ui.h"

//! Main structure for card view
typedef struct HealthCardView HealthCardView;


////////////////////////////////////////////////////////////////////////////////////////////////////
// API Functions
//

//! Creates a HealthCardView
//! @param health_data A pointer to the health data being given this view
//! @return A pointer to the newly allocated HealthCardView
HealthCardView *health_card_view_create(HealthData *health_data);

//! Destroy a HealthCardView
//! @param health_card_view A pointer to an existing HealthCardView
void health_card_view_destroy(HealthCardView *health_card_view);

//! Push a HealthCardView to the window stack
//! @param health_card_view A pointer to an existing HealthCardView
void health_card_view_push(HealthCardView *health_card_view);

//! Mark the card view as dirty so it is refreshed
//! @param health_card_view A pointer to an existing HealthCardView
void health_card_view_mark_dirty(HealthCardView *health_card_view);
