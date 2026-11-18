# LVGL

[LVGL](https://github.com/lvgl/lvgl) is a graphics library with widgets,
layouts, styles and animations. PebbleOS builds it for
{doc}`applications <../development/build_system/applications>` that draw
their own interface, with a port to the display and the input devices of
the board. The watch firmware does not use it.

## Enabling it

`CONFIG_LVGL=y` builds the library and the port, on top of the display:

```kconfig
CONFIG_DISPLAY=y
CONFIG_LVGL=y
```

LVGL is configured with its own Kconfig options, the `CONFIG_LV_*` symbols,
which the build reads from the library and applies without an `lv_conf.h`.
An application sets them in its `prj.conf`, as any other option:

```kconfig
CONFIG_LV_FONT_MONTSERRAT_24=y
CONFIG_LV_USE_DEMO_WIDGETS=y
```

Only what these options turn on is compiled, and LVGL turns most of its
widgets, both themes and every colour format of its renderer on by
default. An application that turns off what it does not use builds faster
and is smaller: the {doc}`hello world <../samples/third_party/lvgl/hello_world>`
sample does, starting from what LVGL's own minimal preset,
`configs/defconfigs/minimal.defconfig`, turns off. What is compiled and not
used is left out of the image by the linker.

The display, input and file system drivers of LVGL, its renderers other
than the software one and what needs libraries PebbleOS does not have are
not built, whatever their options say.

## Using it

`pbl_lvgl_init()`, from `pbl_lvgl.h`, initializes LVGL, its display and its
input devices. After it the application uses the LVGL API, and calls
`lv_timer_handler()` for LVGL to render and to read input:

```c
#include <pbl/kernel/delay.h>
#include <pbl_lvgl.h>

int main(void) {
  int ret = pbl_lvgl_init();
  if (ret != 0) {
    return ret;
  }

  lv_label_set_text(lv_label_create(lv_screen_active()), "Hello");

  while (true) {
    pbl_sleep_ms(lv_timer_handler());
  }
}
```

LVGL is built without locking (`CONFIG_LV_OS_NONE`): every call into it,
`lv_timer_handler()` included, has to come from the same thread. It renders
on that thread, which needs a stack to match: 8 KiB for a simple interface,
set with `CONFIG_KERNEL_MAIN_STACK_SIZE` when it is the main thread.

## The port

Display
: LVGL renders in the format of the display where it has one to match.
  On a display with an RGB565 frame it renders straight into it, the frame
  the board reserves if it has one, and the port sends the rows that
  changed once a frame is complete. If the scratch heap has room for a
  second frame, as it does in the external RAM of a board with
  `CONFIG_SCRATCH_HEAP_SHARED=n`, LVGL renders the next frame in it while
  the first is being sent. On a mono display it renders in 1 bit per
  pixel, with its monochrome theme, in bands of
  `CONFIG_LVGL_DRAW_BUFFER_ROWS` rows that the port copies into a frame it
  owns, reversing the bits of each byte: what is lighter than
  `CONFIG_LV_DRAW_SW_I1_LUM_THRESHOLD` is white. LVGL has no format for the
  displays with 2 bits per component: for those it renders the bands in
  RGB565 and the port keeps the two most significant bits of each
  component. The buffers the board does not reserve come from the kernel
  heap. The renderer needs the format it is asked for,
  `CONFIG_LV_DRAW_SW_SUPPORT_RGB565` or `CONFIG_LV_DRAW_SW_SUPPORT_I1`:
  both are on by default.

GPU
: With `CONFIG_GPU`, and unless `CONFIG_LVGL_DRAW_GPU` is turned off, the
  port adds a draw unit that hands what the GPU of the board can take to
  the GPU class: fills, including rounded ones where the corners are not
  drawn, images and layers in its formats, with their opacity, rotation
  and scale, and the glyphs of labels from
  `CONFIG_LVGL_DRAW_GPU_GLYPH_MIN_PIXELS` up, which leaves text at reading
  sizes to software, where it is as fast. Everything else, areas under `CONFIG_LVGL_DRAW_GPU_MIN_PIXELS`
  and what the GPU refuses are drawn by LVGL in software, so the result is
  the same with any GPU, or none.

Touchscreen
: With `CONFIG_TOUCH`, the touchscreen is a pointer,
  `pbl_lvgl_get_pointer()`, and the port keeps it powered.

Keys
: With `CONFIG_INPUT`, the keys are a keypad, `pbl_lvgl_get_keypad()`: Up
  and Down are `LV_KEY_PREV` and `LV_KEY_NEXT`, Select is `LV_KEY_ENTER` and
  Back is `LV_KEY_ESC`. The port creates the default group and assigns it
  to the keypad, so widgets that take input can be reached with the keys
  as they are created.

Time
: The tick is the kernel uptime.

Memory
: LVGL allocates from its own pool of `CONFIG_LV_MEM_SIZE` bytes, 64 KiB
  unless set.

The samples under `samples/third_party/lvgl` show it in use:
{doc}`../samples/third_party/lvgl/hello_world` and
{doc}`../samples/third_party/lvgl/input` with as little of LVGL as they
need, and the demos that come with it:
{doc}`../samples/third_party/lvgl/benchmark`,
{doc}`../samples/third_party/lvgl/keypad_encoder`,
{doc}`../samples/third_party/lvgl/music`,
{doc}`../samples/third_party/lvgl/render`,
{doc}`../samples/third_party/lvgl/stress` and
{doc}`../samples/third_party/lvgl/widgets`.
