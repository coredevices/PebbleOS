/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/touch/touch_click_synth.h"

#include "applib/event_service_client.h"
#include "drivers/button_id.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "kernel/ui/modals/modal_manager.h"
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
  bool active;       // sensor-hold state (driven by focus events)
  bool finger_down;
  bool dragging;     // moved past the tap threshold -> not a tap
  int16_t start_x;
  int16_t start_y;
  RtcTicks down_ticks;
} SynthState;

static SynthState s_state;

static EventServiceInfo s_focus_event_info;

// Kernel-side copy of the focused window's action bar, published by applib via
// syscall. Read when routing a tap to a bar zone.
static ActionBarSynthDescriptor s_action_bar;

void touch_click_synth_set_action_bar(const ActionBarSynthDescriptor *descriptor) {
  s_action_bar = *descriptor;
}

// If the tap at (x, y) lands on the action bar, return the button for its
// vertical zone (top -> UP, middle -> SELECT, bottom -> DOWN), falling back to
// SELECT for a zone whose icon is absent. Returns false if there is no action
// bar or the tap is outside it, leaving the caller's default (SELECT) in place.
static bool prv_action_bar_button_for_tap(int16_t x, int16_t y, ButtonId *button_out) {
  if (!s_action_bar.present) {
    return false;
  }
  const GRect *f = &s_action_bar.frame;
  if (x < f->origin.x || x >= f->origin.x + f->size.w ||
      y < f->origin.y || y >= f->origin.y + f->size.h ||
      f->size.h <= 0) {
    return false;
  }

  // Split the bar into three equal vertical zones.
  const int16_t zone = (int16_t)(((int32_t)(y - f->origin.y) * 3) / f->size.h);
  ButtonId button;
  uint8_t icon_bit;
  if (zone <= 0) {
    button = BUTTON_ID_UP;
    icon_bit = ACTION_BAR_SYNTH_ICON_UP;
  } else if (zone == 1) {
    button = BUTTON_ID_SELECT;
    icon_bit = ACTION_BAR_SYNTH_ICON_SELECT;
  } else {
    button = BUTTON_ID_DOWN;
    icon_bit = ACTION_BAR_SYNTH_ICON_DOWN;
  }
  *button_out = (s_action_bar.icon_mask & icon_bit) ? button : BUTTON_ID_SELECT;
  return true;
}

// A focused (non-unfocused) modal receives button events in
// launcher_handle_button_event, so the bridge must synthesize for it too (e.g.
// the notification-detail popup, action menus, dialogs). Mirrors the
// is_modal_focused test there.
static bool prv_modal_focused(void) {
  return modal_manager_get_enabled() &&
         !(modal_manager_get_properties() & ModalProperty_Unfocused);
}

// The synthesis gate. Re-evaluated live at touch time (not just cached from the
// focus event) so a delayed or missed PEBBLE_APP_DID_CHANGE_FOCUS_EVENT can't
// leave the bridge unable to respond while the sensor is on. Active whenever a
// focused modal is up, or the foreground process is a watchapp (not a
// watchface). Synthesized buttons route to whoever holds focus (app or modal)
// via the normal kernel button path, so a bare "not the watchface" test here is
// sufficient regardless of app/modal focus split.
static bool prv_gate_ok(void) {
  return prv_modal_focused() || !app_manager_is_watchface_running();
}

static void prv_set_active(bool active) {
  if (active == s_state.active) {
    return;
  }
  s_state.active = active;
  s_state.finger_down = false;
  s_state.dragging = false;
  if (!active) {
    // Leaving the foreground (e.g. back to the watchface): drop any stale action
    // bar descriptor so it can't route taps in the next context.
    s_action_bar = (ActionBarSynthDescriptor) { 0 };
  }
  // Hold or release the touch sensor. This subscription is excluded from
  // touch_has_app_subscribers() so it never suppresses our own synthesis.
  touch_set_synthesis_enabled(active);
}

static void prv_focus_handler(PebbleEvent *e, void *context) {
  // Drive the sensor hold from focus changes. This matches the "on while
  // viewing an app or a focusing modal" power profile of the
  // ScrollLayer/MenuLayer touch support: on a return to the watchface with no
  // focused modal, the sensor is released. A focusing modal (notification
  // popup, action menu, ...) takes focus from the app -> in_focus is false for
  // it, so it is included explicitly. The synthesis decision itself is a live
  // re-check (prv_gate_ok), so this event only needs to be roughly right.
  const bool watchapp_focused = e->app_focus.in_focus && !app_manager_is_watchface_running();
  prv_set_active(watchapp_focused || prv_modal_focused());
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
  // Live gate re-check: robust to focus-event timing (e.g. a focusing modal's
  // did_focus fires only when its entrance transition completes) as long as the
  // sensor is on. If it isn't a valid synthesis target right now, drop the event
  // and any in-flight gesture state.
  if (!prv_gate_ok()) {
    s_state.finger_down = false;
    s_state.dragging = false;
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
        // Tap: SELECT by default, or UP/SELECT/DOWN when it lands on the action
        // bar (by vertical zone), if it lifted off quickly enough.
        const uint64_t elapsed_ms =
            ((uint64_t)(rtc_get_ticks() - s_state.down_ticks) * 1000) / RTC_TICKS_HZ;
        if (elapsed_ms < SYNTH_TAP_MAX_MS) {
          ButtonId button = BUTTON_ID_SELECT;
          prv_action_bar_button_for_tap(s_state.start_x, s_state.start_y, &button);
          prv_synthesize_click(button);
        }
        break;
      }

      // A drag past the threshold is a swipe. A predominantly vertical swipe
      // maps to UP/DOWN; horizontal swipes are ignored for now (BACK is a
      // later phase). Content-scroll convention (matches ScrollLayer/MenuLayer
      // touch and common touch UIs): flicking the finger up reveals content
      // further down -> DOWN button; flicking down reveals earlier content ->
      // UP button. So the button is opposite the finger's travel direction.
      const int16_t dx = event->x - s_state.start_x;
      const int16_t dy = event->y - s_state.start_y;
      if (ABS(dy) > ABS(dx) && ABS(dy) >= SYNTH_TAP_MOVE_THRESHOLD_PX) {
        prv_synthesize_click(dy < 0 ? BUTTON_ID_DOWN : BUTTON_ID_UP);
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
