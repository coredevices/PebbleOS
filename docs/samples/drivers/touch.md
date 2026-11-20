# Touch: Contacts

This sample powers the touchscreen, `TOUCH`, up for 10 seconds and prints
where each touch starts and ends, by polling `pbl_touch_get_last()`. It
also prints the properties of the panel. `pbl_touch_enable()` and
`pbl_touch_disable()` count users: the controller stays powered until the
last user disables it.

The driver also reports touches to the input subsystem as events, which the
{doc}`input events sample <../subsys/input/events>` shows. That is what the
system uses, because polling can miss a touch shorter than the polling period.

On a watch, touch the screen while the sample runs. On the emulator, use
`pbl emulator touch` or `pbl emulator swipe`; on a native board, click in
the display window.

```{pebbleos-sample}
:path: samples/drivers/touch
:sources: src/main.c
:output:
```
