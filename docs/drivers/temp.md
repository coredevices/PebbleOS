# Temperature sensor

A temperature sensor reads a temperature, such as that of the die.

| | |
| --- | --- |
| Directory | `drivers/temp` |
| Header | `pbl/drivers/temp.h` |
| Kconfig | `CONFIG_TEMP`, enabled by the application |
| Role | `TEMP` |
| Sample | {doc}`../samples/drivers/temp` |
| API | {apidoc}`API reference <drivers_temp>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 | `nordic,nrf52832-temp` | `CONFIG_TEMP_NRF5` |
| SiFli SF32LB | `sifli,sf32lb52-tsen` | `CONFIG_TEMP_SF32LB` |
| Simulator | — | `CONFIG_TEMP_SIM` |
