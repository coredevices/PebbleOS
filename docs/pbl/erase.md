# `pbl erase`

Erase parts of the flash of a watch connected through a development kit:
regions of the board's flash layout, by name, or offset ranges in it.

```{warning}
Erasing `FILESYSTEM` wipes the pairings, apps and settings stored on the
watch.
```

```shell
pbl erase --region FILESYSTEM --region SHARED_PRF_STORAGE
```

The regions are the partitions in the
`include/pbl/board/partitions_*.h` header of the board's layout.
`--region NAME:SIZE` erases only the first `SIZE` bytes of a
region, and `--range OFFSET:SIZE` any range; both can be repeated. Ranges
start at offset 0 of the flash, and must be aligned to its erase size
(4 KiB).

## Options

```{pbl-command} erase
```
