# Non-volatile memory

A non-volatile memory, such as eFuses, holds small data the rest of the system
reads, as a flat byte address space. A consumer takes a named cell from a
devicetree `nvmem-cells` entry.

| | |
| --- | --- |
| Directory | `drivers/nvmem` |
| Header | `pbl/drivers/nvmem.h` |
| Kconfig | `CONFIG_NVMEM`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_nvmem>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF FICR | `nordic,nrf52832-ficr`, `nordic,nrf54l15-ficr` | `CONFIG_NVMEM_NRF_FICR` |
| SiFli SF32LB eFuse | `sifli,sf32lb57-efuse`, `sifli,sf32lb52-efuse` | `CONFIG_NVMEM_SF32LB` |
