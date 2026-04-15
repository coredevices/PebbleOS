/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"

#include <time.h>
#include <stdint.h>

typedef uint8_t TouchIdx;
typedef int16_t TouchPressure;

//! Touch event type
typedef enum TouchEventType {
  TouchEvent_Touchdown,
  TouchEvent_Liftoff,
  TouchEvent_PositionUpdate,
  TouchEvent_PressureUpdate,
} TouchEventType;

//! Touch event
typedef struct TouchEvent {
  TouchEventType type;
  TouchIdx index;
  GPoint start_pos;
  GPoint diff_pos;
  int64_t start_time_ms;
  int64_t diff_time_ms;
  TouchPressure start_pressure;
  TouchPressure diff_pressure;
} TouchEvent;
