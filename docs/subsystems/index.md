# Subsystems

Subsystems are self-contained OS components under `subsys/`, each with its
public API in `include/pbl/<name>/`. Most follow the same pattern: users
declare what they need next to their own code, the subsystem collects
it at link time, and a Kconfig option selects the backend.

```{toctree}
:maxdepth: 1

analytics.md
bluetooth.md
coredump.md
crc.md
fs.md
i18n.md
input.md
logging.md
pp/index.md
profiler.md
shell.md
console.md
task_watchdog.md
```

The {apidoc}`API reference <subsys>` documents every subsystem's interface.
