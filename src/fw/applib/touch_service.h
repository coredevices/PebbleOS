/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/touch/gesture_event.h"
#include "pbl/services/touch/touch_event.h"

#include <stdbool.h>
#include <stdint.h>

//! Callback for touch events.
//! @param event The touch event data
//! @param context User-provided context
typedef void (*TouchServiceHandler)(const TouchEvent *event, void *context);

//! Subscribe to touch events. The touch sensor is enabled while subscribed.
//! @param handler Callback invoked for each touch event
//! @param context User context passed to the callback
void touch_service_subscribe(TouchServiceHandler handler, void *context);

//! Unsubscribe from touch events. The touch sensor is disabled if no other subscribers remain.
void touch_service_unsubscribe(void);

//! @return true if touch input is currently being delivered to apps.
//! Returns false on platforms without a touchscreen, or when touch has been
//! disabled system-wide (future feature). Apps can poll this (for example on
//! window appear) to avoid looking broken when touch is unavailable.
bool touch_service_is_enabled(void);

//! Callback for gesture events.
//! @param event The gesture event data
//! @param context User-provided context
typedef void (*GestureServiceHandler)(const GestureEvent *event, void *context);

//! Subscribe to gesture events. The touch sensor is enabled while subscribed.
//! @param handler Callback invoked for each gesture event
//! @param context User context passed to the callback
void gesture_service_subscribe(GestureServiceHandler handler, void *context);

//! Unsubscribe from gesture events. The touch sensor is disabled if no other subscribers remain.
void gesture_service_unsubscribe(void);
