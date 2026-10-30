Flash Drivers
-------------

Flash is a device class, `struct pbl_flash` in `include/pbl/drivers/flash.h`.
The class core in `flash.c` serializes access, enforces the protected range,
runs erases (blocking or asynchronous, suspended around reads and writes) and
has a coredump mode that relies on no OS service. Drivers only move data and
start erases through `struct pbl_flash_ops`.

The board defines its flash with the `PBL_FLASH_*_DEFINE()` macro of its driver
and exports it as `FLASH`. NOR parts are described in `nor.h` and defined in
their own file.

Real hardware runs the SPI NOR driver, `spi_nor.c`, over a SPI memory
controller (`drivers/spi_mem`): the controller only runs command, address,
dummy and data operations, and maps reads, while the NOR protocol (SFDP, 4-byte
addressing, quad enable, suspend, deep power-down, OTP) lives in the NOR driver.
The OTP area is the part's security registers; `otp.c` exposes it as a linear
range, and boards name the cells they keep factory data in.
