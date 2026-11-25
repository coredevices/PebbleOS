# RAM

A RAM device is memory outside the SoC's own, mapped at an address its driver
finds out. Its contents are only kept while it has users: the first powers
it on, and once the last is gone the next deep sleep powers it off.

| | |
| --- | --- |
| Directory | `drivers/ram` |
| Header | `pbl/drivers/ram.h` |
| Kconfig | `CONFIG_RAM`, enabled by the application |
| Role | `RAM` |
| Sample | {doc}`../samples/drivers/ram` |
| API | {apidoc}`API reference <drivers_ram>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| HyperRAM | `cypress,hyperram` | `CONFIG_RAM_HYPERRAM` |
| QEMU PSRAM | `qemu,pebble-psram` | `CONFIG_RAM_QEMU` |
