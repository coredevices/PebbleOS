# Subsystems

Subsystems are self-contained OS components under `subsys/`, each with its
public API in `include/pbl/<name>/`. Most follow the same pattern: users
declare what they need next to their own code, the subsystem collects
it at link time, and a Kconfig option selects the backend.

```{toctree}
:maxdepth: 1

bluetooth.md
crc.md
fs.md
input.md
logging.md
shell.md
task_watchdog.md
```

The {apidoc}`API reference <subsys>` documents every subsystem's interface.
