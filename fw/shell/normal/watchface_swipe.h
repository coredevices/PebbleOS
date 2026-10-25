/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/ui/recognizer/swipe.h>

// A standalone recognizer; it never subscribes to the touch service.
typedef struct WatchfaceSwipe {
  alignas(void *) uint8_t storage[SWIPE_RECOGNIZER_STATIC_SIZE];
  Recognizer *recognizer;
  bool live;
} WatchfaceSwipe;

void watchface_swipe_init(WatchfaceSwipe *swipe);
SwipeDirection watchface_swipe_handle_touch(WatchfaceSwipe *swipe, const TouchEvent *event,
                                            bool eligible);
void watchface_swipe_cancel(WatchfaceSwipe *swipe);
