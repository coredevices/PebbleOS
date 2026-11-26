# USB: Mass storage

This sample is a USB disk: a RAM disk (`CONFIG_DISK_RAM`) that the sample
formats at boot with a FAT12 file system, labelled `PEBBLEOS`, holding a
`README.TXT`, then gives the host as the logical unit of the mass storage
function. The host mounts it as any USB drive; what it writes is lost when
the board resets. With `CONFIG_SAMPLE_READ_ONLY`, the host may only read it.

Connect the board's USB device port to a computer; on macOS:

```shell
diskutil list external
cat /Volumes/PEBBLEOS/README.TXT
dd if=/dev/urandom of=/Volumes/PEBBLEOS/test.bin bs=1k count=128 && sync
diskutil eject PEBBLEOS
```

The integration tests in `tests/integration/usb/test_msc.py` run on this
sample (see {doc}`../../../development/tests/integration`).

```{pebbleos-sample}
:path: samples/subsys/usb/msc
:sources: src/main.c
:output:
```
