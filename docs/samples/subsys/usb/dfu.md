# USB: Firmware upgrade

This sample is a USB device in DFU mode: `dfu-util` downloads an image to a
1 MiB flash partition, labelled `dfu` in the sample's board overlay, and
uploads it back. The flash is erased and written by a work item while the
host polls the device, not from the controller's interrupt.

Connect the board's USB device port to a computer, then:

```shell
dfu-util -l
head -c 300000 /dev/urandom > image.bin
dfu-util -d 1209:0001 -a 0 -D image.bin
dfu-util -d 1209:0001 -a 0 -U readback.bin -Z 300000
cmp image.bin readback.bin
```

```{pebbleos-sample}
:path: samples/subsys/usb/dfu
:sources: src/main.c
:output:
```
