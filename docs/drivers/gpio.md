# GPIO

A GPIO port is an SoC port or a peripheral with GPIOs on it. A pin carries
its wiring flags (polarity, pulls, open drain), so callers deal in logical
levels. GPIO ports also provide interrupt lines, through the
{doc}`interrupt controller <intc>` class.

| | |
| --- | --- |
| Directory | `drivers/gpio` |
| Header | `pbl/drivers/gpio.h` |
| Kconfig | `CONFIG_GPIO`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_gpio>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 | `nordic,nrf52832-gpio` | `CONFIG_GPIO_NRF5` |
| SiFli SF32LB | `sifli,sf32lb57-gpio`, `sifli,sf32lb52-gpio` | `CONFIG_GPIO_SF32LB` |
| ST STM32 | `st,stm32h743-pinctrl` | `CONFIG_GPIO_STM32` |
| QEMU GPIO | `qemu,pebble-gpio` | `CONFIG_GPIO_QEMU` |
| Nordic nPM1300 GPIO | `nordic,npm1300` | `CONFIG_GPIO_NPM1300` |
