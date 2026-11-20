# Backlight: Brightness and color

This sample ramps the backlight from off to full brightness and back in steps
of 5, then, at full brightness, sets it to red, green, blue and white. A
backlight without color refuses colors with `-ENOTSUP`, and the sample says
so. It turns the backlight off at the end.

On a watch, the screen lights up and fades out. The emulator shows the
backlight on its window.

```{pebbleos-sample}
:path: samples/drivers/backlight
:sources: src/main.c
:output:
```
