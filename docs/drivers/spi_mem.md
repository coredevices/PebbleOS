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
| SiFli SF32LB MPI | `sifli,sf32lb57-mpi`, `sifli,sf32lb52-mpi` | `CONFIG_SPI_MEM_SF32LB_MPI` |
