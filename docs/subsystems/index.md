# Subsystems

Subsystems are self-contained OS components under `subsys/`, each with its
public API in `include/pbl/<name>/`. Most follow their Zephyr counterpart:
users declare what they need next to their own code, the subsystem collects
it at link time, and a Kconfig option selects the backend.

```{apigroups} subsys
bluetooth: bluetooth
crc: crc
input: input
logging: logging
shell: shell
task_wdt: task_watchdog
```

```{toctree}
:hidden:
:maxdepth: 1

bluetooth.md
crc.md
input.md
logging.md
shell.md
task_watchdog.md
```
