/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/touch/touch_click_synth.h"

#include "applib/event_service_client.h"
#include "drivers/button_id.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "pbl/services/touch/touch.h"
#include "process_management/app_manager.h"
#include "pbl/util/math.h"

// Movement threshold that separates a tap from a swipe, and the tap timeout.
// Both mirror ScrollLayer's touch handling so the gesture definitions are
// consistent across the codebase.
#define SYNTH_TAP_MOVE_THRESHOLD_PX 10
#define SYNTH_TAP_MAX_MS 300

// Transient tap-gesture state plus the foreground gate. A single physical
// touchscreen means only one gesture is ever in flight, so a single
// file-static instance suffices.
typedef struct {
  bool active;       // gate: a watchapp is the focused foreground process
  bool finger_down;
  bool dragging;     // moved past the tap threshold -> not a tap
  int16_t start_x;
  int16_t start_y;
  RtcTicks down_ticks;
} SynthState;

static SynthState s_state;

static EventServiceInfo s_focus_event_info;

static void prv_set_active(bool active) {
  if (active == s_state.active) {
    return;
  }
  s_state.active = active;
  s_state.finger_down = false;
  s_state.dragging = false;
  // Hold or release the touch sensor. This subscription is excluded from
  // touch_has_app_subscribers() so it never suppresses our own synthesis.
  touch_set_synthesis_enabled(active);
}

static void prv_focus_handler(PebbleEvent *e, void *context) {
  // Active only while a watchapp (not a watchface) is the focused foreground
  // process. This matches the "on while viewing an app" power profile of the
  // ScrollLayer/MenuLayer touch support: on a modal overlay or a return to the
  // watchface, focus is lost and the sensor is released.
  const bool active = e->app_focus.in_focus && !app_manager_is_watchface_running();
  prv_set_active(active);
}

static void prv_synthesize_click(ButtonId button_id) {
  // A button down immediately followed by a button up registers as exactly one
  // single-click in the click recognizer, and flows through the normal button
  // path (kernel arbitration -> the focused app/modal click manager).
  PebbleEvent down = {
    .type = PEBBLE_BUTTON_DOWN_EVENT,
    .button = { .button_id = button_id },
  };
  PebbleEvent up = {
    .type = PEBBLE_BUTTON_UP_EVENT,
    .button = { .button_id = button_id },
  };
  event_put(&down);
  event_put(&up);
}

void touch_click_synth_handle_touch(const TouchEvent *event) {
  if (!s_state.active) {
    return;
  }

  switch (event->type) {
    case TouchEvent_Touchdown:
      s_state.finger_down = true;
      s_state.dragging = false;
      s_state.start_x = event->x;
      s_state.start_y = event->y;
      s_state.down_ticks = rtc_get_ticks();
      break;

    case TouchEvent_PositionUpdate:
      if (s_state.finger_down && !s_state.dragging) {
        if (ABS(event->x - s_state.start_x) > SYNTH_TAP_MOVE_THRESHOLD_PX ||
            ABS(event->y - s_state.start_y) > SYNTH_TAP_MOVE_THRESHOLD_PX) {
          s_state.dragging = true;  // committed to a drag, not a tap
        }
      }
      break;

    case TouchEvent_Liftoff: {
      if (!s_state.finger_down) {
        break;
      }
      s_state.finger_down = false;

      // Arbitration (shared by tap and swipe): if the focused app consumes
      // touch itself (ScrollLayer/MenuLayer or a custom touch_service
      // subscriber), don't synthesize anything, to avoid a double action.
      if (touch_has_app_subscribers()) {
        break;
      }

      if (!s_state.dragging) {
        // Tap -> SELECT, if it lifted off quickly enough.
        const uint64_t elapsed_ms =
            ((uint64_t)(rtc_get_ticks() - s_state.down_ticks) * 1000) / RTC_TICKS_HZ;
        if (elapsed_ms < SYNTH_TAP_MAX_MS) {
          prv_synthesize_click(BUTTON_ID_SELECT);
        }
        break;
      }

      // A drag past the threshold is a swipe. A predominantly vertical swipe
      // maps to UP/DOWN; horizontal swipes are ignored for now (BACK is a
      // later phase). Direct spatial mapping: a swipe toward the top of the
      // screen is UP, toward the bottom is DOWN.
      const int16_t dx = event->x - s_state.start_x;
      const int16_t dy = event->y - s_state.start_y;
      if (ABS(dy) > ABS(dx) && ABS(dy) >= SYNTH_TAP_MOVE_THRESHOLD_PX) {
        prv_synthesize_click(dy < 0 ? BUTTON_ID_UP : BUTTON_ID_DOWN);
      }
      break;
    }
  }
}

void touch_click_synth_init(void) {
  s_state = (SynthState){ 0 };
  s_focus_event_info = (EventServiceInfo) {
    .type = PEBBLE_APP_DID_CHANGE_FOCUS_EVENT,
    .handler = prv_focus_handler,
  };
  event_service_client_subscribe(&s_focus_event_info);
}
