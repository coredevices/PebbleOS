/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "watchface_swipe.h"

#ifdef CONFIG_TOUCH

#include <applib/ui/recognizer/recognizer_private.h>
#include <applib/ui/recognizer/touch_nav.h>

static void prv_swipe_event(const Recognizer *recognizer, RecognizerEvent event) {
  // Completion is read synchronously after dispatch.
}

void watchface_swipe_init(WatchfaceSwipe *swipe) {
  *swipe = (WatchfaceSwipe){};
  swipe->recognizer = swipe_recognizer_init_static(
      swipe->storage, prv_swipe_event, nullptr,
      SwipeDirection_Up | SwipeDirection_Down | SwipeDirection_Left | SwipeDirection_Right);
}

void watchface_swipe_cancel(WatchfaceSwipe *swipe) {
  swipe->live = false;
  if (swipe->recognizer) {
    recognizer_reset(swipe->recognizer);
  }
}

SwipeDirection watchface_swipe_handle_touch(WatchfaceSwipe *swipe, const TouchEvent *event,
                                            bool eligible) {
  if (event->type == TouchEvent_Touchdown) {
    watchface_swipe_cancel(swipe);
    swipe->live =
        eligible && !event->non_navigational && event->y >= TOUCH_NAV_STATUS_BAR_DEAD_ZONE_PX;
  }
  if (!eligible || event->non_navigational) {
    watchface_swipe_cancel(swipe);
  }
  if (!swipe->live || !recognizer_is_active(swipe->recognizer)) {
    return SwipeDirection_None;
  }

  recognizer_handle_touch_event(swipe->recognizer, event);
  const SwipeDirection direction = swipe_recognizer_get_direction(swipe->recognizer);
  if (event->type == TouchEvent_Liftoff || !recognizer_is_active(swipe->recognizer)) {
    swipe->live = false;
  }
  return direction;
}

#endif
