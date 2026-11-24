# Timer service

SDK revision 119 adds the `TimerService` to the Foundation API. An app can
start a disposable countdown timer, list every running timer, and cancel
timers. When a timer expires the watch shows a popup with the timer's duration
and vibrates, whether or not the app is still running.

```c
TimerServiceId id;
TimerServiceCreateResult result = timer_service_create(5 * 60, &id);
if (result == TimerServiceCreateErrNoSpace) {
  // All TIMER_SERVICE_MAX_TIMERS slots are in use.
}

// Later, if the user changes their mind:
timer_service_cancel(id);
```

`timer_service_create()` takes a duration in seconds, from 1 to
`TIMER_SERVICE_MAX_DURATION_S` (24 hours). It returns
`TimerServiceCreateErrInvalidArgument` for anything outside that range and
`TimerServiceCreateErrNoSpace` when all `TIMER_SERVICE_MAX_TIMERS` slots are
taken. `timer_service_can_create()` answers the capacity question only; it
does not check the permission and is advisory.

## Listing timers

An app can recover timers created earlier, including timers created by other
apps, for example after being relaunched:

```c
TimerServiceInfo timers[TIMER_SERVICE_MAX_TIMERS];
uint8_t count = timer_service_get_timers(timers, TIMER_SERVICE_MAX_TIMERS);
for (uint8_t i = 0; i < count; i++) {
  // timers[i].description is e.g. "5:00" or "1:00:00".
  // timers[i].remaining_s is the time left.
  timer_service_cancel(timers[i].id);
}
```

Every running timer is returned, soonest expiry first. The `description` field
is the original duration formatted for display, so an app can show the shared
timer list and let the wearer pick one to cancel. `remaining_s` is the time
left at the moment of the call. Timers being cancelled are not returned.

Timers live in memory only. A reset clears them, and they do not appear in the
Alarms app. Each timer records the UUID of the app that created it, so
`timer_service_cancel_all()` only stops that app's timers; it does not need the
`Timers` permission. Listing and cancelling by ID need the permission; an app
with it sees every timer and can cancel any of them. Uninstalling the app does
not cancel its timers.

Declare `manifestVersion: 2` and `usesPermissions: ["Timers"]` in the
`pebble` block of `package.json`:

```json
"usesPermissions": [{"name": "Timers", "reason": "Starts a tea timer"}]
```

The phone app shows the permission at install time and syncs the user's
decision in the AppDB permission field (bit 4). The grant is stored on the
watch, keyed by the app UUID, so it survives disconnecting from the phone or
rebooting the watch. An app without the grant gets
`TimerServiceCreateErrPermissionDenied` and cannot list or cancel timers;
`timer_service_cancel_all()` still works on its own timers. The watch never
prompts on its own.

The phone app must know the `Timers` permission before it can show or grant
it. Until CoreApp ships that support, the watch treats every app as denied.
The SDK build checks that an app linking `timer_service_create()`,
`timer_service_get_timers()` or `timer_service_cancel()` declares the
permission, the same way it checks the Health and Alarms APIs.
`timer_service_cancel_all()` is exempt because it only touches the caller's
own timers.

## Expiry popup

The popup reuses the alarm popup's layout and vibration cadence, on the timer
app's `GColorPictonBlue` background (`GColorWhite` on black-and-white
platforms). Any button dismisses it, and it dismisses itself after one minute
of ringing. It shows the timer's original duration, not the remaining time.

Expiries that arrive while the popup is already visible, while an alarm is
active, or while modal popups are disabled are queued and shown in turn. If
an alarm takes over the screen, the timer popup is dismissed and its expiry is
queued to show after the alarm. A fired timer keeps its slot until its popup
has been shown and dismissed, so queued expiries count against
`TIMER_SERVICE_MAX_TIMERS`; if every slot is waiting for its notification,
`timer_service_create()` returns `TimerServiceCreateErrNoSpace` until they
are shown. Alarms that fire while modal popups are disabled are deferred and
shown when the lockout ends.

The popup uses the existing `ALARM_CLOCK_LARGE` resource, the animated
alarm-clock PDC (`Pebble_80x80_Alarm_clock.pdc`) that the timer app also plays
when a timer expires. Reusing it keeps one copy of the animation in the
firmware; a later timer-specific animation would need its own resource.

On platforms whose SDK is frozen before revision 119, the functions are stubs:
`timer_service_can_create()` returns false, `timer_service_create()` returns
`TimerServiceCreateErrUnavailable`, `timer_service_cancel()` returns false,
`timer_service_cancel_all()` returns 0 and `timer_service_get_timers()` returns
0.
