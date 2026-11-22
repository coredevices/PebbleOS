# SD: Card as a disk

This sample opens the SD card as a disk and prints its size and whether its
first sector holds a boot signature. It then writes a pattern to the last
sector, usually unused, reads it back and puts the sector back as it was, and
times reading the first MiB. Without a card in the slot, it says so.

```{pebbleos-sample}
:path: samples/subsys/sd/disk
:sources: src/main.c
:output:
```
