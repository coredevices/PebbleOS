Flash Memory Drivers
--------------------

`include/pbl/drivers/flash.h` is the only flash API. It operates on a
`struct pbl_flash_device`; the generic layer (`flash.c`) owns locking, write
protection, blank checks and the erase engine, while drivers implement
`struct pbl_flash_ops` for a controller.

Devices are instantiated by the drivers themselves, not by boards: each
driver defines its device from Kconfig (`FLASH_NRF5_QSPI_*`,
`FLASH_SF32LB52_MPI_*`, `FLASH_QEMU_*`) and the selected NOR part
(`FLASH_NOR_PART` choice, whose command sets and geometry live in
`nor_part.h`), and exports it as `FLASH`.

After `pbl_flash_coredump_init()` the same API works without locks, timers or
sleeping so that core dumps can be written from a fault handler.
