# Logging

`subsys/logging/` sends leveled log messages to the serial console (or
PULSE) and to a circular buffer in flash. Its public API is
`include/pbl/logging/logging.h`; the {apidoc}`API reference <logging>`
covers every macro.

## Levels

Use the level-named macros as their names suggest: `PBL_LOG_ERR()` and
`PBL_LOG_WRN()` for errors and warnings, `PBL_LOG_DBG()` for routine
lifecycle and state-transition messages, and `PBL_LOG_INFO()` only for
events that deserve attention in a default-level capture. A code path that
can fire repeatedly in normal use must not log at INFO.

## Modules

A source file can belong to a log module with its own Kconfig level.
Declare the level with the Kconfig template:

```kconfig
module = TASK_WDT
module-str = Task watchdog
source "subsys/logging/Kconfig.template.log_level"
```

and bind the file to it:

```c
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DEFINE(task_wdt, CONFIG_TASK_WDT_LOG_LEVEL);

PBL_LOG_DBG("channel %d armed", id);
```

Messages above the module level are compiled out. Files without a module
use the default level.

## Hashing

With `CONFIG_LOG_HASHED`, the default, format strings are replaced by
tokens at build time: the binary only carries a hash and the arguments, and
the console dehashes them with the dictionary the build writes (see
{doc}`../pbl/console`). A hashed message takes at most 7 conversions, 2 of
them `%s`, 32-bit arguments and no `%%`; do not mix `%s` with `%x` or `%X`
in one message.

## Configuration

:`CONFIG_DEFAULT_LOG_LEVEL_<LEVEL>`:
Default log level, where `<LEVEL>` is one of `ERROR`, `WARNING`, `INFO`
(default), `DEBUG` or `DEBUG_VERBOSE`.

:`CONFIG_LOG_HASHED=n`:
Disable log message hashing. This increases ROM usage, but logs no longer
need a dictionary to decode.

:`CONFIG_<MODULE>_LOG_LEVEL_RUNTIME=y`:
Keep all log messages of a module in the image and filter them at runtime,
using `CONFIG_<MODULE>_LOG_LEVEL` as the initial level. The level is
changed with `PBL_LOG_MODULE_LEVEL_SET(name, level)` and read with
`PBL_LOG_MODULE_LEVEL_GET(name)`, where `name` is the one given to
`PBL_LOG_MODULE_DEFINE`. Messages accepted by the module level skip the
serial and flash log level filters. This increases ROM usage for that
module.

The **Settings → System → Debugging → Vibe Log Info** preference sets the
alert UI, vibe score and vibe pattern modules to DEBUG, so their traces
reach bug reports. Normal firmware enables runtime levels for these modules
by default. Disabling the preference restores each module's configured
level; the preference is also applied at boot. Recovery firmware keeps
compile-time logging by default.
