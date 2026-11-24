# ADC

An analog-to-digital converter reads channels in microvolts. A consumer takes
its channel from the `io-channels` property of its devicetree node.

| | |
| --- | --- |
| Directory | `drivers/adc` |
| Header | `pbl/drivers/adc.h` |
| Kconfig | `CONFIG_ADC`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_adc>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB57 GPADC | `sifli,sf32lb57-gpadc` | `CONFIG_ADC_SF32LB57_GPADC` |
