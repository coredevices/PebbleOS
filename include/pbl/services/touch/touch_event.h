/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @brief Touch event type */
typedef enum TouchEventType {
  /** A finger touched the screen. */
  TouchEvent_Touchdown,
  /** The finger left the screen. */
  TouchEvent_Liftoff,
  /** The finger moved while touching the screen. */
  TouchEvent_PositionUpdate,
} TouchEventType;

/** @brief Touch event data, carried directly in PebbleTouchEvent */
typedef struct TouchEvent {
  /** Event type. */
  TouchEventType type : 8;
  /**
   * true when the touch must not drive navigation: the interaction session
   * was inactive at Touchdown (unarmed contact on the idle watchface).
   * Latched on Touchdown and carried across the whole gesture.
   */
  bool non_navigational;
  /** Horizontal position, in screen pixels. */
  int16_t x;
  /** Vertical position, in screen pixels. */
  int16_t y;
} TouchEvent;

_Static_assert(sizeof(TouchEvent) <= 9, "TouchEvent must stay small; it rides inside PebbleEvent");
