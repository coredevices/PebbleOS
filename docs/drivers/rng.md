# RNG

A random number generator returns random words from a hardware entropy
source.

| | |
| --- | --- |
| Directory | `drivers/rng` |
| Header | `pbl/drivers/rng.h` |
| Kconfig | `CONFIG_RNG`, enabled by the application |
| Sample | {doc}`../samples/drivers/rng` |
| API | {apidoc}`API reference <drivers_rng>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 | — | `CONFIG_RNG_NRF5` |
| Nordic nRF54L CRACEN | `nordic,nrf54l15-cracen`, `nordic,nrf54lm20a-cracen` | `CONFIG_RNG_NRF_CRACEN` |
| SiFli SF32LB | `sifli,sf32lb52-trng` | `CONFIG_RNG_SF32LB` |
| SiFli SF32LB | `sifli,sf32lb57-trng` | `CONFIG_RNG_SF32LB` |
| ST STM32 | `st,stm32-rng` | `CONFIG_RNG_STM32` |
| QEMU | — | `CONFIG_RNG_QEMU` |
| POSIX | — | `CONFIG_RNG_POSIX` |
