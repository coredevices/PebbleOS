# SD cards

`subsys/sd/` brings up the SD card in the slot of the board's SD host
controller, its `sdhc` role, and gives it as a disk of 512-byte sectors,
`SD_DISK` in `include/pbl/sd/sd.h`. It is built with `CONFIG_SD`.

```{mermaid}
flowchart TD
  user["Disk user<br/>pbl/drivers/disk.h"]
  sd["Card stack<br/>subsys/sd/sd.c"]
  sdhc["SD host controller<br/>pbl/drivers/sdhc.h"]
  drv["Controller driver<br/>STM32 SDMMC"]
  user -- "open, sectors" --> sd
  sd -- "commands, blocks" --> sdhc
  sdhc --> drv
```

## The card

Opening the disk brings the card up when the slot has one, as the SD physical
layer specification describes: idle, the interface condition, the operating
condition until the card leaves its power-up, at 400 kHz on one data line,
then its identification, its relative address and its size from the CSD. The
card is then selected and set to the widest bus the slot is wired for. When the
slot runs faster than the 25 MHz of the default speed and the card takes the
switch function, with high speed among its access modes, the card switches to
it and is clocked at up to 50 MHz, otherwise at up to 25 MHz. Opening it again
checks it is still in the slot; without one, opening fails with `-ENOMEDIUM`.

Sectors are read with single or multiple block reads, a multiple one ended
with a stop, and written the same way, the stack then waiting until the card
has programmed them. High-capacity cards take sector numbers, standard ones
byte addresses.

## Controllers

A controller driver implements `struct pbl_sdhc_ops`: it sets the bus clock
and width, runs a command with an optional data phase of whole blocks, and
tells whether a card is in the slot, from its card detect switch. A controller
that moves fewer blocks in one command says so in `max_blocks`, and the stack
splits the transfers in as many commands. One that only moves several blocks
from buffers aligned to `multi_block_align` gets the others one block at a
time.
