# Temperature sensor

A temperature sensor reads a temperature, such as that of the die.

| | |
| --- | --- |
| Directory | `drivers/sensor` |
| Header | `pbl/drivers/sensor.h` |
| Kconfig | `CONFIG_TEMP`, enabled by the application, selects `CONFIG_SENSOR` |
| Role | `TEMP` |
| Sample | {doc}`../samples/drivers/temp` |
| API | {apidoc}`API reference <drivers_sensor>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 | `nordic,nrf52832-temp` | `CONFIG_TEMP_NRF5` |
| SiFli SF32LB | `sifli,sf32lb52-tsen` | `CONFIG_TEMP_SF32LB` |
| STM32H7 ADC | `st,stm32h7-adc` | `CONFIG_TEMP_STM32H7_ADC` |
| Simulator | — | `CONFIG_TEMP_SIM` |
