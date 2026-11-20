# LVGL: Stress

This sample runs the stress demo that comes with
{doc}`LVGL <../../../third_party/lvgl>`: one round of objects of every kind
created, animated and deleted while others are still animating. It ends
after the round, with the most memory LVGL had in use.

```{pebbleos-sample}
:path: samples/third_party/lvgl/stress
:sources: src/main.c
:output:
```
