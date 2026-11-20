# RTC: Getting and setting the time

This sample reads the board's RTC, sets it to a fixed date, waits two seconds
to see it count on, then puts the time back, plus the time it spent on the
sample date. An RTC counts UTC seconds since the epoch on its own, through
resets; the kernel keeps the running wall clock, which the firmware's clock
service sets from the RTC at boot.

On a watch, the first line shows the time it last kept. The emulator starts
its RTC at the host's local time.

```{pebbleos-sample}
:path: samples/drivers/rtc
:sources: src/main.c
:output:
```
