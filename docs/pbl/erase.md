# `pbl erase`

Erase parts of the flash of a watch connected through a development kit:
regions of the board's flash layout, by name, or address ranges in it.

```{warning}
Erasing `FILESYSTEM` wipes the pairings, apps and settings stored on the
watch.
```

```shell
pbl erase --region FILESYSTEM --region SHARED_PRF_STORAGE
```

The regions are the ones in the `fw/flash_region` header of the board's
flash part. `--region NAME:SIZE` erases only the first `SIZE` bytes of a
region, and `--range ADDRESS:SIZE` any range; both can be repeated. Ranges
use the layout's addresses, and must be aligned to the flash's erase size
(4 KiB).

## Options

```{pbl-command} erase
```
