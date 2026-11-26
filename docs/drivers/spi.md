# SPI

An SPI bus carries full-duplex transfers to peripherals, with the chip select
asserted throughout.

| | |
| --- | --- |
| Directory | `drivers/spi` |
| Header | `pbl/drivers/spi.h` |
| Kconfig | `CONFIG_SPI`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_spi>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 SPI Controller | `nordic,nrf52832-spim` | `CONFIG_SPI_NRF5` |
| Nordic nRF54L SPI Controller | `nordic,nrf54l15-spim` | `CONFIG_SPI_NRF54L` |
