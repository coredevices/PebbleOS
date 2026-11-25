# Regulator

A regulator is a power rail that consumers enable and disable with a use
count. Enabling waits the rail's enable time, so it is up when the call
returns.

| | |
| --- | --- |
| Directory | `drivers/regulator` |
| Header | `pbl/drivers/regulator.h` |
| Kconfig | `CONFIG_REGULATOR`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_regulator>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB52 PMUC regulators | `sifli,sf32lb52-pmuc` | `CONFIG_REGULATOR_SF32LB52_PMUC` |
| SiFli SF32LB57 PMUC regulators | `sifli,sf32lb57-pmuc` | `CONFIG_REGULATOR_SF32LB57_PMUC` |
| Nordic nPM1300 regulators | `nordic,npm1300` | `CONFIG_REGULATOR_NPM1300` |
| Fixed regulators | `regulator-fixed` | `CONFIG_REGULATOR_FIXED` |
