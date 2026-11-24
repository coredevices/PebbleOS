# Reset

A reset controller drives reset lines. A consumer takes its line from the
devicetree and asserts or pulses it.

| | |
| --- | --- |
| Directory | `drivers/reset` |
| Header | `pbl/drivers/reset.h` |
| Kconfig | `CONFIG_RESET`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_reset>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB RCC | `sifli,sf32lb57-hpsys-rcc`, `sifli,sf32lb52-hpsys-rcc` | `CONFIG_RESET_SF32LB` |
