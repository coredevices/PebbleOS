# SPI memory

A SPI memory controller runs command, address, dummy and data operations
against a memory, such as a NOR flash or a PSRAM, without knowing its
protocol, which belongs to the memory's driver.

| | |
| --- | --- |
| Directory | `drivers/spi_mem` |
| Header | `pbl/drivers/spi_mem.h` |
| Kconfig | `CONFIG_SPI_MEM`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_spi_mem>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF5 QSPI | `nordic,nrf52840-qspi` | `CONFIG_SPI_MEM_NRF5_QSPI` |
| Memory on a SPI bus | `nordic,nrf54l15-spim` | `CONFIG_SPI_MEM_SPI` |
| SiFli SF32LB MPI | `sifli,sf32lb57-mpi`, `sifli,sf32lb52-mpi` | `CONFIG_SPI_MEM_SF32LB_MPI` |

## Memories on a SPI bus

A memory on a plain SPI controller, a child of its node in the devicetree,
gets a controller of this class from `drivers/spi_mem/spi.c`
(`CONFIG_SPI_MEM_SPI`), bound to the SPI controller's node when it has a
`jedec,spi-nor` child. It runs single-line operations as two transfers with
the chip select of the memory asserted throughout: the command, address and
dummy bytes, then the data, which goes through a 256-byte buffer the
controller can reach, so operations carry at most 256 bytes. Its
spec lists the SPI controllers it runs on.
