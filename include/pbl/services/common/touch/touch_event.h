/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

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
  int16_t x;
  int16_t y;
} TouchEvent;
