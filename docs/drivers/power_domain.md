# Power domain

A power domain runs at a performance state, such as the voltage mode its
clocks need. A consumer sets the state it needs before it raises its clocks.

| | |
| --- | --- |
| Directory | `drivers/power_domain` |
| Header | `pbl/drivers/power_domain.h` |
| Kconfig | `CONFIG_POWER_DOMAIN`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_power_domain>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| SiFli SF32LB PMUC power domains | `sifli,sf32lb52-pmuc`, `sifli,sf32lb57-pmuc` | `CONFIG_POWER_DOMAIN_SF32LB_PMUC` |
