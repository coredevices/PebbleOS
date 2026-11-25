# System timer

The system timer drives the kernel's notion of time: a clock source, a
free-running counter the kernel reads, and a clock event device, which
interrupts after a given number of cycles for the next tick or timeout. One
device may provide both, and both keep running in every idle state. The SoC
selects the drivers of its timers.

| | |
| --- | --- |
| Directory | `drivers/timer` |
| Header | `pbl/drivers/timer.h` |
| API | {apidoc}`API reference <drivers_timer>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Arm SysTick | — | `CONFIG_TIMER_ARM_SYSTICK` |
| QEMU | — | `CONFIG_TIMER_QEMU` |
| Nordic nRF5 RTC | — | `CONFIG_TIMER_NRF5_RTC` |
| SiFli SF32LB LPTIM | — | `CONFIG_TIMER_SF32LB_LPTIM` |
| ST STM32 LPTIM | — | `CONFIG_TIMER_STM32_LPTIM` |
| POSIX | — | `CONFIG_TIMER_POSIX` |
