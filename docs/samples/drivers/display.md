# Display: Full and partial updates

This sample draws colour bars across the display, `DISPLAY`, and sends the
whole frame with `pbl_display_update_sync()`. Three seconds later it draws a
checkerboard over the lower third and sends only those rows with
`pbl_display_update()`, which runs in the background and calls back from the
display's interrupt when done. The frame buffer is allocated to the size of
the display, in its pixel format: one byte per pixel in colour, one bit in
mono, where anything but black shows as white.

Watch the screen, on a watch or in the emulator's window.

```{pebbleos-sample}
:path: samples/drivers/display
:sources: src/main.c
:output:
```
