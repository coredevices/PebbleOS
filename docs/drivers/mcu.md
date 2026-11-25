# MCU information

MCU information reports facts about the SoC, such as its unique ID.

| | |
| --- | --- |
| Directory | `drivers/mcu` |
| Header | `pbl/drivers/mcu.h` |
| Kconfig | `CONFIG_MCU`, enabled by the application |
| API | {apidoc}`API reference <drivers_mcu>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 | — | `CONFIG_MCU_NRF5` |
| SiFli SF32LB | — | `CONFIG_MCU_SF32LB` |
| ST STM32 | — | `CONFIG_MCU_STM32` |
| QEMU | — | `CONFIG_MCU_QEMU` |
| POSIX | — | `CONFIG_MCU_POSIX` |
