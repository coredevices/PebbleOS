# LVGL: Benchmark

This sample runs the benchmark demo that comes with
{doc}`LVGL <../../../third_party/lvgl>`: a series of scenes, from an empty
screen to rotated images and a screen full of widgets, each measured for a
few seconds. When the last one ends it prints the frame rate, the CPU load
and the render and flush times of each scene, and their averages.

It takes about a minute.

```{pebbleos-sample}
:path: samples/third_party/lvgl/benchmark
:sources: src/main.c
:output:
```
