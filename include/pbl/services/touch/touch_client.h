/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "touch_event.h"

/**
 * @addtogroup services_touch
 * @{
 */

/**
 * @brief Touch event callback.
 *
 * @param event Touch event.
 * @param context Callback context.
 */
typedef void (*TouchEventHandler)(const TouchEvent *event, void *context);

/**
 * @brief Dispatch touch events to a handler.
 *
 * @param touch_idx Index of the touch to dispatch events for.
 * @param event_handler Callback to dispatch touch events to.
 * @param context Callback context.
 */
void touch_dispatch_touch_events(TouchIdx touch_idx, TouchEventHandler event_handler,
                                 void *context);

/** @} */
