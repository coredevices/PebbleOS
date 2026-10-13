# Input

`subsys/input/` carries input events from the drivers to the code that
consumes them. Its public API is `include/pbl/input/input.h`. It is modelled
on the Zephyr input subsystem: drivers report events without knowing who
consumes them, and listeners are defined next to the code they drive and
collected at link time.

## Events

An event is a type, a code and a value:

- `PBL_INPUT_EV_KEY`: a key or button, with value 1 when pressed and 0 when
  released. The codes `PBL_INPUT_KEY_BACK`, `_UP`, `_SELECT` and `_DOWN`
  are the watch buttons, set per button in the board configuration;
  `PBL_INPUT_BTN_TOUCH` is the touch contact.
- `PBL_INPUT_EV_ABS`: an absolute axis, `PBL_INPUT_ABS_X` and `_Y` in pixels
  for touch.
- `PBL_INPUT_EV_GES`: a gesture recognized by the device (tap, double tap,
  palm), located at the report's `ABS_X` and `ABS_Y`.

Events are grouped in reports, the last one carrying `sync`. A touch sample
is `BTN_TOUCH`, `ABS_X`, `ABS_Y` (sync); a gesture is `ABS_X`, `ABS_Y`,
`GES` (sync); a button is a single `KEY` event with `sync`.

## Delivery

`pbl_input_report()` calls every listener before it returns, in the context
of the report: the debounce ISR for buttons, the system task for touch, the
calling thread for injected input. There is no queue or thread of its own;
the listeners that need to defer work already have the kernel event queue.

Listeners are registered with `PBL_INPUT_CALLBACK_DEFINE()` and must not
block. Those that use APIs that are not ISR-safe check `pbl_in_isr()`.

```c
static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  if (evt->type == PBL_INPUT_EV_KEY && evt->code == PBL_INPUT_KEY_BACK && evt->value) {
    ...
  }
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, NULL);
```

The firmware has two listeners:

- `fw/kernel/input_buttons.c` turns key events into
  `PEBBLE_BUTTON_DOWN_EVENT` / `PEBBLE_BUTTON_UP_EVENT`, so the click
  recognizers and everything above them are unchanged.
- The touch service (`fw/services/touch/touch.c`) collects each report and
  passes it to `touch_handle_update()` and `touch_handle_gesture()`.

The SELECT+BACK hard reset stays in the button drivers: it has to work when
every thread is wedged, so it runs in the debounce timer ISR that also times
how long the buttons are held.

## Injection

Injected input goes through the same path as the hardware:

- `input report <key|abs|ges> <code> <value> [sync]` reports any event, and
  `input dump on` logs every event (`CONFIG_INPUT_SHELL`).
- `button raw <btn> <0|1>` reports a button transition.
- `button click` / `hold` / `multi` and the remote input endpoint
  (`fw/kernel/remote_input.c`) report their button presses.

Injected touch gestures (`touch_handle_injected_update()`) still go straight
to the touch service, which arbitrates between them and the physical sensor.
