# LVGL: Music player

This sample runs the music player demo that comes with
{doc}`LVGL <../../../third_party/lvgl>`: a screen full of artwork, text and
an animated spectrum that plays by itself and follows the touchscreen. It
enables the demo and the fonts it uses with LVGL's own options in its
`prj.conf`, and what the board needs, such as its GPU, in
`boards/<board>.conf`.

Unlike the other samples it does not end.

```{pebbleos-sample}
:path: samples/third_party/lvgl/music
:sources: src/main.c
:output:
```
