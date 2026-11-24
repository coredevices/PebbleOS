# Timer service

SDK revision 119 adds the `TimerService` to the Foundation API. An app can
start a disposable countdown timer and cancel the timers it created. When a
timer expires the watch shows a popup with the timer's duration and vibrates,
whether or not the app is still running.

```c
TimerServiceId id;
TimerServiceCreateResult result = timer_service_create(5 * 60, &id);
if (result == TimerServiceCreateErrNoSpace) {
  // All 8 timer slots are in use.
}

// Later, if the user changes their mind:
timer_service_cancel(id);
```

`timer_service_create()` takes a duration in seconds, from 1 to
`TIMER_SERVICE_MAX_DURATION_S` (24 hours). It returns
`TimerServiceCreateErrInvalidArgument` for anything outside that range and
`TimerServiceCreateErrNoSpace` when all 8 slots are taken.
`timer_service_can_create()` answers the capacity question only; it does not
check the permission and is advisory.

Timers live in memory only. A reset clears them, and they do not appear in the
Alarms app. Each timer records the UUID of the app that created it, so
`timer_service_cancel()` and `timer_service_cancel_all()` only affect that
app's timers. Cancelling does not need the `Timers` permission, so an app can
always stop the timers it created. Uninstalling the app does not cancel its
timers.

Declare `manifestVersion: 2` and `usesPermissions: ["Timers"]` in the
`pebble` block of `package.json`:

```json
"usesPermissions": [{"name": "Timers", "reason": "Starts a tea timer"}]
```

The phone app shows the permission at install time and syncs the user's
decision in the AppDB permission field (bit 4). The grant is stored on the
watch, keyed by the app UUID, so it survives disconnecting from the phone or
rebooting the watch. An app without the grant gets
`TimerServiceCreateErrPermissionDenied`; the watch never prompts on its own.

The phone app must know the `Timers` permission before it can show or grant
it. Until CoreApp ships that support, the watch treats every app as denied.
The SDK build checks that an app linking `timer_service_create()` declares the
permission, the same way it checks the Health and Alarms APIs.

## Expiry popup

The popup reuses the alarm popup's layout and vibration cadence, on the timer
app's `GColorPictonBlue` background (`GColorWhite` on black-and-white
platforms). Any button dismisses it, and it dismisses itself after one minute
of ringing. It shows the timer's original duration, not the remaining time.

The popup uses the existing `ALARM_CLOCK_LARGE` resource, the animated
alarm-clock PDC (`Pebble_80x80_Alarm_clock.pdc`) that the timer app also plays
when a timer expires. Reusing it keeps one copy of the animation in the
firmware; a later timer-specific animation would need its own resource.

On platforms whose SDK is frozen before revision 119, the functions are stubs:
`timer_service_can_create()` returns false, `timer_service_create()` returns
`TimerServiceCreateErrUnavailable`, `timer_service_cancel()` returns false and
`timer_service_cancel_all()` returns 0.
