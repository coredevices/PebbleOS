# File system: Files

This sample mounts PFS, the {doc}`file system <../../../subsystems/fs>` of
the external flash, at the root, and works with files on it.

PFS files have a fixed size: `pbl_fs_truncate()` gives it after creating the
file, and a write past it fails. A file opened with `PBL_FS_O_REPLACE` is a
new version that replaces the old one only once closed, so a reset in the
middle leaves either. The sample counts its runs in such a file, read back at
the start of each run; a watch on another file reports it closed after
writing.

The flash may hold other files, so the sample lists only its own.

```{pebbleos-sample}
:path: samples/subsys/fs/files
:sources: src/main.c
:output:
```
