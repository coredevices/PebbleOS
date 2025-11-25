/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "unobstructed_area_service.h"

#include "applib/event_service_client.h"

//! @internal
typedef struct UnobstructedAreaState {
  EventServiceInfo event_info;
  UnobstructedAreaHandlers handlers;
  GRect area;
  void *context;
  bool is_subscribed;
  bool is_changing;
} UnobstructedAreaState;

//! @internal
//! Initializes the unobstructed area state
//! @param state Unobstructed area state belonging to the consuming task
//! @param current_y The current obstruction y to initialize the unobstructed area with
void unobstructed_area_service_init(UnobstructedAreaState *state, int16_t current_y);

//! @internal
//! Deinitializes the unobstructed area state
//! @param state Unobstructed area state belonging to the consuming task
void unobstructed_area_service_deinit(UnobstructedAreaState *state);

//! @internal
//! Returns the last unobstructed area
//! @param state Unobstructed area state belonging to the consuming task
//! @param area The GRect to write the unobstructed area to
void unobstructed_area_service_get_area(UnobstructedAreaState *state, GRect *area);

//! @internal
//! Subscribe to be notified when the app's unobstructed area changes.
//! @param state Unobstructed area state belonging to the consuming task
//! @param handlers The handlers that should be called when the unobstructed area changes.
//! @param context A user-provided context that will be passed to the callback handlers.
void unobstructed_area_service_subscribe(UnobstructedAreaState *state,
                                         const UnobstructedAreaHandlers *handlers,
                                         void *context);

//! @internal
//! @param state Unobstructed area state belonging to the consuming task
//! Unsubscribe from notifications about changes to the app's unobstructed area.
void unobstructed_area_service_unsubscribe(UnobstructedAreaState *state);
