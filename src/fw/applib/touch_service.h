/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/common/touch/touch_event.h"

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
