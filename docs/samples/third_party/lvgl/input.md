# LVGL: Input

This sample puts a counter and two buttons on the display and lets them be
used for 30 seconds, with what the board has: Up and Down move the focus
between the buttons and Select clicks the focused one, and a touch clicks
the one under the finger. Each click runs the event callback of the
button, which updates the label and prints the count.

The keys reach {doc}`LVGL <../../../third_party/lvgl>` with `CONFIG_INPUT`,
and the touchscreen with `CONFIG_TOUCH`, which the sample sets in
`boards/<board>.conf` for the boards that have one.

On the emulator, click the buttons in its window or use the arrow keys;
nothing comes when run by `pbl samples`, and it reports 0 clicks.

```{pebbleos-sample}
:path: samples/third_party/lvgl/input
:sources: src/main.c
:output:
```
