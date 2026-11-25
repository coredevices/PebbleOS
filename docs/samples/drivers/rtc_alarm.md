# RTC: Alarm

This sample arms the board's RTC alarm three seconds ahead and waits for it.
The alarm calls back from its interrupt, so the callback only gives a
semaphore, which `main()` waits on. An armed alarm also wakes the system from
power-off, on the SoCs whose power-off keeps the RTC running.

```{pebbleos-sample}
:path: samples/drivers/rtc_alarm
:sources: src/main.c
:output:
```
