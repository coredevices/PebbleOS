# Pin control

A pin controller routes peripheral signals to pins and sets their pad options.
A consumer selects its default state, from the board, before using its
peripheral, and its sleep state, if any, after powering it down.

| | |
| --- | --- |
| Directory | `drivers/pinctrl` |
| Header | `pbl/drivers/pinctrl.h` |
| Kconfig | `CONFIG_PINCTRL`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_pinctrl>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB52 pin controller | `sifli,sf32lb52-pinctrl` | `CONFIG_PINCTRL_SF32LB52` |
| SiFli SF32LB57 pin controller | `sifli,sf32lb57-pinctrl` | `CONFIG_PINCTRL_SF32LB57` |
| ST STM32 pin controller | `st,stm32h743-pinctrl` | `CONFIG_PINCTRL_STM32` |
