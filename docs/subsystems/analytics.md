# Analytics

`subsys/analytics/` collects firmware metrics into hourly heartbeats. Its
public API is `include/pbl/analytics/analytics.h`. `CONFIG_ANALYTICS` enables
it in normal firmware; recovery firmware has no data logging and leaves it
out, which compiles the macros below to nothing beyond evaluating their
values.

## Metrics

Metrics are declared in `include/pbl/analytics/analytics.def` as unsigned,
signed, scaled, timer or string metrics, and updated by name from anywhere,
apps included, since the functions behind the macros are syscalls:

```c
PBL_ANALYTICS_ADD(button_pressed_count, 1);

PBL_ANALYTICS_TIMER_START(backlight_on_time_ms);
PBL_ANALYTICS_TIMER_STOP(backlight_on_time_ms);
```

## Collectors

A metric that is only worth computing once per period comes from a collector:
a function next to the state it reads, called on the system task right before
each heartbeat. Collectors are gathered at link time, so analytics does not
know about them and a firmware without the code has no collector to stub:

```c
static void prv_collect_pfs_stats(void) {
  PBL_ANALYTICS_SET_UNSIGNED(pfs_space_free_kb, get_available_pfs_space() / 1024);
}
PBL_ANALYTICS_COLLECTOR_DEFINE(prv_collect_pfs_stats);
```

They run in no particular order, so a collector must not depend on another.

## Heartbeats

The native backend, `subsys/analytics/native.c`, keeps the current values.
Each heartbeat it writes them as one packed record to a data logging session,
which sends it to the phone, and resets them for the next period. Running timers
keep running across heartbeats.

The record's layout follows `analytics.def`, and it carries the firmware's build
ID. `tools/analytics_heartbeat_layout.py` extracts the layout from the
matching ELF file, for parsers.

## Shell

| Command | Description |
| --- | --- |
| `analytics heartbeat` | Collect and log a heartbeat now |
| `analytics metrics` | Dump the current metrics |
