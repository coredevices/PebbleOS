# Logging: Levels

This sample has a {doc}`log module <../../../subsystems/logging>` of its own.
Its `Kconfig` sources the log level template, which defines
`CONFIG_SAMPLE_LOG_LEVEL` and `CONFIG_SAMPLE_LOG_LEVEL_RUNTIME`; `prj.conf`
picks the debug level, and `src/sensor.c` joins the module with
`PBL_LOG_MODULE_DECLARE()`.

It logs once at each level: the verbose message is dropped. With a runtime
level, `PBL_LOG_MODULE_LEVEL_SET()` then raises it to warnings, and only the
error and the warning remain. Without one, the level is fixed when building,
and messages above it are not even in the image.

```{pebbleos-sample}
:path: samples/subsys/logging/levels
:sources: Kconfig prj.conf src/main.c src/sensor.c
:output:
```
