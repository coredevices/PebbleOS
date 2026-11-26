# Disk

A disk is block storage read and written by whole sectors of one size,
numbered from 0: what a file system or the USB mass storage function (see
{doc}`USB <../subsystems/usb>`) sits on. The class only checks that the
sectors are on the disk, that it is ready and that it takes writes; how a
driver stores them, erase blocks and caching included, is its own. A write may
stay in the driver until `pbl_disk_sync()`. The class does not serialize
calls: a driver that cannot take them from several threads at once serializes
them itself.

| | |
| --- | --- |
| Directory | `drivers/disk` |
| Header | `pbl/drivers/disk.h` |
| Kconfig | `CONFIG_DISK`, enabled by the application |
| API | {apidoc}`API reference <drivers_disk>` |
