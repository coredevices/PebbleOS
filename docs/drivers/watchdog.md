# Watchdog

A hardware watchdog resets the system unless it is fed in time, and reports
the cause of the last reset.

| | |
| --- | --- |
| Directory | `drivers/watchdog` |
| Header | `pbl/drivers/watchdog.h` |
| Kconfig | `CONFIG_WATCHDOG`, enabled by the application |
| API | {apidoc}`API reference <drivers_watchdog>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 | — | `CONFIG_WATCHDOG_NRF5` |
| SiFli SF32LB | `sifli,sf32lb57-iwdt`, `sifli,sf32lb52-wdt` | `CONFIG_WATCHDOG_SF32LB` |
| ST STM32 IWDG | `st,stm32-iwdg` | `CONFIG_WATCHDOG_STM32_IWDG` |
| QEMU Watchdog | — | `CONFIG_WATCHDOG_QEMU` |
