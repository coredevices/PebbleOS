/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Touch event type
typedef enum TouchEventType {
  TouchEvent_Touchdown,
  TouchEvent_Liftoff,
  TouchEvent_PositionUpdate,
} TouchEventType;

//! Touch event data, carried directly in PebbleTouchEvent
typedef struct TouchEvent {
  TouchEventType type:8;
  //! true when the touch must not drive navigation: the wake tap that turned
  //! the screen on, or a DnD-suppressed touch where no wake happened. Latched
  //! on Touchdown and carried across the whole gesture. Sits in the padding
  //! after type:8, so x/y offsets and the struct size are unchanged.
  bool non_navigational;
  int16_t x;
  int16_t y;
} TouchEvent;

_Static_assert(sizeof(TouchEvent) <= 9, "TouchEvent must stay small; it rides inside PebbleEvent");
