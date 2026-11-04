# Logging

`subsys/logging/` passes leveled log messages to backends, such as the
serial console and a circular buffer in flash. Its public API is
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

## Backends

A backend receives every message at or below its level, in the context of
the caller. It is defined anywhere in the firmware with
`PBL_LOG_BACKEND_DEFINE()` from `include/pbl/logging/backend.h`, and
provides a `process` operation, plus optional `init` (run by
`pbl_log_init()`) and `panic` (run by `pbl_log_panic()` before a software
reset) operations:

```c
#include <pbl/logging/backend.h>

static void prv_process(const struct pbl_log_backend *backend,
                        const struct pbl_log_msg *msg) {
  ...
}

static const struct pbl_log_backend_api s_api = {
  .process = prv_process,
};

PBL_LOG_BACKEND_DEFINE(mine, s_api, LOG_LEVEL_INFO, true);
```

Messages carry flags: `PBL_LOG_MSG_ASYNC` when the message may be written
later, and `PBL_LOG_MSG_FAULT` for messages from fault handlers, where OS
services are unavailable. Messages logged by apps carry the app UUID. A
backend skips the messages it cannot handle in the current context.

Backends start enabled unless defined otherwise, and are enabled, disabled
and given a level at runtime with `pbl_log_backend_enable()`,
`pbl_log_backend_disable()` and `pbl_log_backend_set_level()`. The `log
backends` shell command lists them, and `log level <backend> [level]` reads
or sets a level.

| Backend | Option | Writes |
|---|---|---|
| `uart` | `CONFIG_LOG_BACKEND_UART` | Text lines on the serial console, above the shell prompt |
| `flash` | `CONFIG_LOG_BACKEND_FLASH` | The debug flash region, one generation per boot; not app or fault messages |
| `app_log` | `CONFIG_SERVICE_APP_LOG_ENDPOINT` | App messages to the phone, while it asks for them |

Each backend in `subsys/logging/backends/` takes its initial level from
`CONFIG_LOG_BACKEND_<BACKEND>_LEVEL_<LEVEL>`.

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
backend level filters. This increases ROM usage for that
module.

The **Settings → System → Debugging → Vibe Log Info** preference sets the
alert UI, vibe score and vibe pattern modules to DEBUG, so their traces
reach bug reports. Normal firmware enables runtime levels for these modules
by default. Disabling the preference restores each module's configured
level; the preference is also applied at boot. Recovery firmware keeps
compile-time logging by default.
