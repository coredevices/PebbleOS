# GPU: Fills and blits

This sample draws into the frame of the display with the GPU, `GPU`: an
opaque fill, a fill blended over it, a sprite with its own alpha, the same
at half opacity and then rotated, scaled and both, around its centre. It
reads pixels back to check each result, times each call, and sends the
frame to the display at the end.

What a GPU cannot do it refuses with `-ENOTSUP` before drawing anything,
which the sample reports instead of failing.

```{pebbleos-sample}
:path: samples/drivers/gpu
:sources: src/main.c
:output:
```
