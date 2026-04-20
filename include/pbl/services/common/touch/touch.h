/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "touch_event.h"

#include <stdbool.h>

typedef enum TouchState {
  TouchState_FingerUp,
  TouchState_FingerDown,
} TouchState;

void touch_init(void);

//! Enable or disable the kernel's touch subscription used for the touch backlight feature.
//! When disabled, the touch sensor is only active if apps have subscribed to touch events.
void touch_set_backlight_enabled(bool enabled);

//! Pass a touch update to the service (called by the touch driver)
//! @param touch_state whether or not the screen is touched
//! @param x x position of touch
//! @param y y position of touch
void touch_handle_update(TouchState touch_state, int16_t x, int16_t y);

//! Reset the touch service.
void touch_reset(void);
