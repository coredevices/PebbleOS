# Vibration motor

A vibration motor runs at a strength until stopped, with active braking and
calibration where the part has them.

| | |
| --- | --- |
| Directory | `drivers/vibe` |
| Header | `pbl/drivers/vibe.h` |
| Kconfig | `CONFIG_VIBE`, enabled by the application |
| Role | `VIBE` |
| Sample | {doc}`../samples/drivers/vibe` |
| API | {apidoc}`API reference <drivers_vibe>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Awinic AW86225 | `awinic,aw86225` | `CONFIG_VIBE_AW86225` |
| Awinic AW86233 | `awinic,aw86233` | `CONFIG_VIBE_AW8623X` |
| TI DRV2604 | `ti,drv2604` | `CONFIG_VIBE_DRV2604` |
| Simulator | — | `CONFIG_VIBE_SIM` |
