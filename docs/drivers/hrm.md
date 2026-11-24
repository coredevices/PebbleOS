# Heart rate monitor

A heart rate monitor measures heart rate and blood oxygen with an optical
sensor, and reports its readings to the HRM manager service.

| | |
| --- | --- |
| Directory | `drivers/hrm` |
| Header | `pbl/drivers/hrm.h` |
| Kconfig | `CONFIG_HRM`, enabled by the application |
| API | {apidoc}`API reference <drivers_hrm>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Goodix GH3X2X | — | `CONFIG_HRM_GH3X2X` |
| QEMU | — | `CONFIG_HRM_QEMU` |
