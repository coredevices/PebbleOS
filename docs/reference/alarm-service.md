# Alarm service

SDK revision 118 adds alarm management to the existing read-only AlarmService
API. An app can inspect the wearer's alarms, create a new one, update it,
enable or disable it, and delete it.

```c
AlarmServiceInfo info = {
  .hour = 7,
  .minute = 30,
  .kind = AlarmServiceKindWeekdays,
  .vibrate = true,
};
AlarmServiceId id;
AlarmServiceCreateResult result = alarm_service_create(&info, &id);
if (result == AlarmServiceCreateErrNoSpace) {
  // All 10 alarm slots are in use.
}
```

`alarm_service_can_create()` reports whether a slot is free. It only answers
the capacity question, does not check either permission, and is advisory:
`alarm_service_create()` must still handle `AlarmServiceCreateErrNoSpace`.
Allocation and persistence happen under one lock, so a create that loses a
race for the last slot returns `AlarmServiceCreateErrNoSpace` instead of
succeeding. A store failure returns `AlarmServiceCreateErrInternal`.
`id_out` is set to `ALARM_SERVICE_INVALID_ID` whenever creation fails.
The alarms app uses the same underlying `alarm_can_schedule()` check.

Smart alarms use activity tracking when it is available, but can be created
at any time. While tracking is off, a smart alarm fires at its configured time
instead of entering the smart-alarm snooze path. Tracking changes reconcile the
schedule: if tracking starts at or after the smart window has begun, the
pending occurrence is kept at its configured time rather than skipped.

## Reading alarms

Use `alarm_service_get_ids()` to list the configured alarms. Pass NULL to
query the count, then call it with a buffer:

```c
const size_t max = alarm_service_get_ids(NULL, 0);
AlarmServiceId *ids = malloc(max * sizeof(*ids));
if (ids) {
  size_t count = alarm_service_get_ids(ids, max);
  if (count > max) {
    count = max;
  }
  for (size_t i = 0; i < count; i++) {
    AlarmServiceAlarm alarm;
    if (alarm_service_get(ids[i], &alarm) != AlarmServiceResultOk) {
      continue;
    }

    // alarm.enabled, alarm.info.hour, alarm.info.minute, alarm.info.kind,
    // alarm.info.custom_days, alarm.info.smart, alarm.info.sound,
    // alarm.info.vibrate and alarm.info.tone are now populated.
  }
  free(ids);
}
```

The returned count is the total number of configured alarms, which may exceed
the `max` buffer size. The order of the IDs is unspecified. Do not derive IDs
from `ALARM_SERVICE_MAX_ALARMS`; use the IDs returned by
`alarm_service_get_ids()`.

An `AlarmServiceId` identifies a slot in the current configured set, not an
alarm forever. Deleting an alarm frees its ID for reuse, and creating an alarm
can return an ID that was previously used. Do not persist IDs across calls;
list the alarms again before a later update or delete.

`alarm_service_get()` returns `AlarmServiceResultErrDoesNotExist` for an unused
ID and `AlarmServiceResultErrInvalidArgument` for an ID outside the
implementation's range.

`AlarmServiceInfo::custom_days` is a bitmask with bit 0 for Sunday through
bit 6 for Saturday. It is only meaningful for `AlarmServiceKindCustom`.

`AlarmServiceInfo` and `AlarmServiceAlarm` are part of the app ABI and are
frozen; new data is exposed through new functions rather than new fields.

An existing alarm can be enabled or disabled without changing its schedule:

```c
AlarmServiceResult result = alarm_service_set_enabled(id, true);
```

`alarm_service_set_enabled()` returns
`AlarmServiceResultErrInvalidArgument` for an ID outside the implementation's
range and `AlarmServiceResultErrDoesNotExist` for an unused ID. Disabling an
alarm also clears any pending snooze and missed-alarm catch-up for it.

`alarm_service_get()` and `alarm_service_set_enabled()` return
`AlarmServiceResultErrInternal` if the alarm store cannot be read or written.
Repeating a no-op enable or disable does not rewrite the alarm or refresh the
timeline.

## Next alarm

`alarm_service_peek_next()` writes the UTC time of the next enabled alarm to
its output and returns true when at least one is scheduled. It returns false
when no enabled alarm is scheduled or the app lacks `AlarmsRead`. Smart alarms
report their configured time, not the start of the early window. The output
may be NULL, in which case the call also returns false.

## Updating and deleting

`alarm_service_update()` replaces an existing alarm's time, recurrence,
smart, sound, vibration, and tone settings. It preserves the alarm's enabled
state; use `alarm_service_set_enabled()` to change that. It requires
`AlarmsWrite`, and for `AlarmServiceKindCustom`, `custom_days` must select at
least one day.

This is a full replacement: fields not set in the `AlarmServiceInfo` are
cleared. An app that needs to preserve the current smart, sound, vibration, or
tone settings must read the alarm first, which requires `AlarmsRead`.

A change to the time, recurrence, smart setting, or custom days also cancels a
pending snooze and a missed-alarm catch-up. Changing only the sound, vibration,
or tone leaves a firing or missed occurrence alone.

```c
AlarmServiceResult result = alarm_service_update(id, &info);
```

`alarm_service_delete()` removes the alarm and its timeline pins immediately.
Deletion cannot be undone. Any configured alarm can be deleted, including one
the wearer created. It requires `AlarmsWrite`.

```c
AlarmServiceResult result = alarm_service_delete(id);
```

Both calls return `AlarmServiceResultErrPermissionDenied` without the grant,
`AlarmServiceResultErrInvalidArgument` for an ID outside the implementation's
range or an invalid update configuration, `AlarmServiceResultErrDoesNotExist`
for an unused ID, and `AlarmServiceResultErrInternal` if the alarm store cannot
be read or written.

## Permission

Declare `manifestVersion: 2` and the permissions your app uses in the
`pebble` block of `package.json`:

```json
"usesPermissions": ["AlarmsRead", "AlarmsWrite"]
```

- `AlarmsRead` allows `alarm_service_get()`, `alarm_service_get_ids()`, and
  `alarm_service_peek_next()`.
- `AlarmsWrite` allows `alarm_service_create()`,
  `alarm_service_set_enabled()`, `alarm_service_update()`, and
  `alarm_service_delete()`.
- `alarm_service_can_create()` remains available without either permission;
  it reveals only whether a slot is free, not any alarm data.

The phone app shows each permission at install time and syncs the user's
decisions in the AppDB permission field: bit 3 for read and bit 4 for write.
The grants are stored on the watch, keyed by app UUID, so they survive
disconnecting from the phone or rebooting the watch.

An app without the needed grant gets
`AlarmServiceResultErrPermissionDenied` from `alarm_service_get()`,
`alarm_service_set_enabled()`, `alarm_service_update()`, and
`alarm_service_delete()`, or `AlarmServiceCreateErrPermissionDenied` from
creation. `alarm_service_get_ids()` has no result code: it returns 0 without
`AlarmsRead`, when no alarms are configured, and when the alarm store cannot
be opened, so 0 does not distinguish denial from an empty or unreadable store.
`alarm_service_peek_next()` returns false without `AlarmsRead`, which is also
what it returns when no enabled alarm is scheduled. The watch never prompts on
its own.

The phone app must know both permissions before it can show or grant them.
Until CoreApp ships that support, the watch treats every app as denied.
Apps built against an SDK older than revision 118 cannot declare either
permission, so they are denied as well; an existing app that calls
`alarm_service_peek_next()` now gets false instead of the next alarm time.
The SDK build checks that an app linking `alarm_service_get()`,
`alarm_service_get_ids()`, or `alarm_service_peek_next()` declares
`AlarmsRead`, and that an app linking `alarm_service_create()`,
`alarm_service_set_enabled()`, `alarm_service_update()`, or
`alarm_service_delete()` declares `AlarmsWrite`, the same way it checks the
Health API.

## Ownership

Alarms created by an app are not app-owned. They appear in the Alarms app,
can be edited or deleted by the wearer, and remain after the app is
uninstalled. The same applies to alarms the wearer created: an app with
`AlarmsRead` can inspect any of them, and an app with `AlarmsWrite` can
create, update, enable, disable, or delete any of them, including ones the
wearer created. There is no API to change an alarm's owner, and deleted alarms
cannot be recovered. The system supports up to 10 alarms in total.

## Frozen platforms

On platforms whose SDK is frozen before revision 118, the functions are
stubs: `alarm_service_can_create()` returns false,
`alarm_service_get_ids()` returns 0, `alarm_service_create()` returns
`AlarmServiceCreateErrUnavailable`, and `alarm_service_get()`,
`alarm_service_set_enabled()`, `alarm_service_update()`, and
`alarm_service_delete()` return `AlarmServiceResultErrUnavailable`.
