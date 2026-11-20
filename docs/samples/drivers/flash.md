# Flash: Geometry and reads

This sample prints the geometry of the board's storage flash, the `FLASH`
device, reads its first 64 KiB in chunks and checks the CRC-32 of what it
read against the one `pbl_flash_crc32()` computes over the same range. It
also checks whether the last subsector is erased.

It only reads: the flash holds the firmware and its data, and has no area a
sample could erase on every board. Writing takes an erased range, from
`pbl_flash_erase()`, then `pbl_flash_write()`; code that stores data uses
the board's partitions (`pbl/board/partitions.h`).

```{pebbleos-sample}
:path: samples/drivers/flash
:sources: src/main.c
:output:
```
