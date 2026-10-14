# Gesture recognizers (touch SDK)

Gesture recognizers let a third-party app react to raw touch gestures — tap,
pan (single-axis drag) and swipe — on touch-capable hardware. They are the
building block for a fully custom scroll or drag interaction, as opposed to
the fixed behaviour of the built-in `ScrollLayer` / `MenuLayer`.

A recognizer is an opaque object created by one of the constructor functions,
attached to a `Window`, and driven by the touch service. When the gesture it
watches for progresses, it invokes the `RecognizerEventCb` you supplied with a
`RecognizerEvent` describing the transition (`Started`, `Updated`, `Completed`,
`Cancelled`). Inside the callback you read the current gesture data with the
per-recognizer getters.

## Exported API

Constructors (each returns an owned `Recognizer *`):

- `tap_recognizer_create(event_cb, user_data)`
- `pan_recognizer_create(event_cb, user_data, axis)` — `axis` is
  `PanAxis_Horizontal` or `PanAxis_Vertical`
- `swipe_recognizer_create(event_cb, user_data, direction_mask)` — a
  bitwise-OR of `SwipeDirection` values

Gesture data getters (call from the event callback):

- Tap: `tap_recognizer_get_tap_point()`
- Pan: `pan_recognizer_get_total_delta()`,
  `pan_recognizer_get_delta_since_start()`,
  `pan_recognizer_get_delta_since_prev()`, `pan_recognizer_get_velocity()`
- Swipe: `swipe_recognizer_get_direction()`, `swipe_recognizer_get_velocity()`

Lifecycle and configuration:

- `window_attach_recognizer(window, recognizer)` /
  `window_detach_recognizer(window, recognizer)`
- `recognizer_destroy(recognizer)` — destroys an *un-owned* recognizer; a
  recognizer owned by a window is destroyed with the window
- `recognizer_set_simultaneous_with(recognizer, cb)` — allow this recognizer
  to be evaluated at the same time as another
- `recognizer_set_fail_after(recognizer, other)` — only evaluate this
  recognizer once `other` has failed (e.g. run a tap only after a pan fails)

## Custom live scroll example

A pan recognizer locked to the vertical axis drives a scroll offset that
follows the finger in real time. `pan_recognizer_get_delta_since_start()` is
exactly `(0, 0)` at the instant the pan Starts, so the content does not jump
when the gesture begins.

```c
#include <pebble.h>

static Window *s_window;
static Layer *s_content_layer;
static Recognizer *s_pan;

static int16_t s_scroll_base;   // committed offset (updated on Complete)
static int16_t s_scroll_offset; // live offset shown while dragging

static void content_update_proc(Layer *layer, GContext *ctx) {
  // Draw your content shifted by s_scroll_offset (negative scrolls up).
  // ... application-specific drawing ...
}

static void pan_handler(const Recognizer *recognizer, RecognizerEvent event) {
  switch (event) {
    case RecognizerEvent_Started:
      // Nothing to do: delta_since_start is (0, 0) here.
      break;
    case RecognizerEvent_Updated: {
      GPoint d = pan_recognizer_get_delta_since_start(recognizer);
      s_scroll_offset = s_scroll_base + d.y;
      layer_mark_dirty(s_content_layer);
      break;
    }
    case RecognizerEvent_Completed:
      // Commit the live offset. You could also kick off inertial scrolling
      // here using pan_recognizer_get_velocity().
      s_scroll_base = s_scroll_offset;
      break;
    case RecognizerEvent_Cancelled:
      // Roll back to the last committed position.
      s_scroll_offset = s_scroll_base;
      layer_mark_dirty(s_content_layer);
      break;
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_content_layer = layer_create(bounds);
  layer_set_update_proc(s_content_layer, content_update_proc);
  layer_add_child(root, s_content_layer);

  // Create a vertical pan and let the window own it.
  s_pan = pan_recognizer_create(pan_handler, NULL, PanAxis_Vertical);
  window_attach_recognizer(window, s_pan);
  // Attaching alone is not enough while the system recognizer set is live: see
  // "The competition contract" below for opting the window out of the bridge.
}

static void window_unload(Window *window) {
  // The window owns s_pan and destroys it; just drop our own layer.
  layer_destroy(s_content_layer);
}

int main(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  app_event_loop();
  window_destroy(s_window);
}
```

## The competition contract

Touch input is dispatched to a single, ordered set of recognizers at a time.
The system installs a **global system recognizer set** (the back-swipe, the
built-in scrolling, etc.) and that set is evaluated **first**. Because the
first set to claim a touch sequence wins, simply attaching your own
recognizers to a window is **not enough** while the system set is live — the
system recognizers consume the gesture before your app ever sees it, and your
recognizers sit in a dead zone.

To take over touch input, an app must opt out of the system set for its
window using `window_set_touch_bridge_disabled()`. With the touch bridge
disabled on a window, the system recognizer set **fails on Touchdown** for
touches on that window, which lets evaluation fall through to the app's own
recognizers — they then receive the full gesture from the first touch event.

The contract in short:

1. System recognizer set is evaluated before app recognizers.
2. While it is live, app recognizers attached to a window never win.
3. `window_set_touch_bridge_disabled()` fails the system set on Touchdown for
   that window, handing every touch to the app's recognizers.

```{note}
`window_set_touch_bridge_disabled()` is exported in this same SDK revision as
the recognizer API above, so a live-scroll app can call it in `window_load` to
opt its window out of the system bridge and take over touch input.
```
