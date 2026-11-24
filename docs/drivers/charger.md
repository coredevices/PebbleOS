# Charger

A charger reports whether external power is present, the charge status and
the battery voltage, current and temperature, and turns charging on and off.
Measuring the battery and turning charging on and off are optional: a charger
that does neither returns `-ENOTSUP`. Every listener added with
`pbl_charger_add_listener()` hears external power come and go.

| | |
| --- | --- |
| Directory | `drivers/charger` |
| Header | `pbl/drivers/charger.h` |
| Kconfig | `CONFIG_CHARGER`, enabled by the application |
| Role | `CHARGER` |
| Sample | {doc}`../samples/drivers/charger` |
| API | {apidoc}`API reference <drivers_charger>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nPM1300 | `nordic,npm1300` | `CONFIG_CHARGER_NPM1300` |
| SiFli SF32LB57 PMU, external power and charge status only | `sifli,sf32lb57-pmuc` | `CONFIG_CHARGER_SF32LB57` |
| Simulator | — | `CONFIG_CHARGER_SIM` |
