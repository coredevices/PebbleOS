# PWM

A PWM controller drives numbered channels. A consumer takes its channel from
a devicetree `pwms` entry, and sets the output by applying a whole state at
once.

| | |
| --- | --- |
| Directory | `drivers/pwm` |
| Header | `pbl/drivers/pwm.h` |
| Kconfig | `CONFIG_PWM`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_pwm>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 | `nordic,nrf52832-pwm` | `CONFIG_PWM_NRF5` |
| Nordic nRF5 RTC PWM | `nordic,nrf52832-rtc-pwm` | `CONFIG_PWM_NRF5_RTC` |
| SiFli SF32LB LPTIM | `sifli,sf32lb52-lptim-pwm` | `CONFIG_PWM_SF32LB_LPTIM` |
