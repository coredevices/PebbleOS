# NimBLE: Heart rate peripheral

This sample is a heart rate sensor, as phones and fitness apps expect one: it
has the standard Heart Rate service of its own, and NimBLE's Battery and
Device Information services, enabled in its `prj.conf`. It advertises the
three services, its name and the heart rate sensor appearance.

Once a peer subscribes to the heart rate measurement, the sample notifies it
of a simulated rate every second, building the notification itself with
`ble_gatts_notify_custom()`. Every ten seconds the battery level drops by
one: NimBLE's Battery service notifies subscribed peers of it.

```{pebbleos-sample}
:path: samples/third_party/nimble/peripheral_hr
:sources: src/main.c prj.conf
:output:
```
