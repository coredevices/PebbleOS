/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @addtogroup services_touch
 * @{
 */

/** @brief Gesture event type. */
typedef enum GestureEventType {
  /** Single tap. */
  GestureEvent_Tap,
  /** Double tap. */
  GestureEvent_DoubleTap,
} GestureEventType;

/** @brief Gesture event data, carried in @c PebbleGestureEvent. */
typedef struct GestureEvent {
  /** Gesture. */
  GestureEventType type : 8;
  /** X coordinate in pixels. */
  int16_t x;
  /** Y coordinate in pixels. */
  int16_t y;
} GestureEvent;

/** @} */
