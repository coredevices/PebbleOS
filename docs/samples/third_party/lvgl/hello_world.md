# LVGL: Hello world

This sample starts {doc}`LVGL <../../../third_party/lvgl>` on the display with
`pbl_lvgl_init()`, puts a label and a spinner on the active screen and
renders them for 5 seconds. Its loop is the whole of what LVGL needs to
run: `lv_timer_handler()` renders what changed and returns how long it can
wait until it is called again.

Watch the screen, on a watch or in the emulator's window.

```{pebbleos-sample}
:path: samples/third_party/lvgl/hello_world
:sources: src/main.c
:output:
```
