# File system

`subsys/fs/` gives access to files regardless of the file system format.
Its public API is `include/pbl/fs/fs.h`; formats plug in through
`include/pbl/fs/fs_sys.h`.

## Model

- A **mount** puts a file system at an absolute path, such as the root (`/`)
  or `/sdcard`.
  `pbl_fs_mount()` takes a caller-owned `struct pbl_fs_mount` with the mount
  point and the format's operations (`struct pbl_fs_ops`). Paths resolve to
  the mount with the longest matching mount point, matching whole path
  components only, and the format sees the rest of the path without the
  leading `/`. A mount hides the paths below its mount point on the file
  system it is mounted over.
- **Files and directories** are caller-owned handles (`struct pbl_fs_file`,
  `struct pbl_fs_dir`): the core keeps no table of open files and does not
  allocate. Every call returns 0, a count or an offset on success and a
  negative errno on failure; `-ENOTSUP` for what a format does not support.
- **Reads** return less than asked at the end of a file. **Writes** past the
  end of a fixed-size file fail with `-EFBIG`.
- **`PBL_FS_O_REPLACE`** writes a new, empty version of a file that replaces
  the current one on close; until then, and across a reboot, readers see the
  current version.
- **Watches** (`pbl_fs_watch_add()`) run a callback when a file opened for
  writing is closed or when a file is removed, whatever the format.

## Pebble File System

PFS, the wear-leveled file system on the external NOR flash, is the root file
system: the normal firmware mounts it at `/` on boot. Its implementation is
private to the subsystem, in `subsys/fs/pfs/`: `pfs.c` holds the on-flash
layout and the wear-leveling strategy (round-robin page allocation tracking
the last written page), `flash_translation.c` joins the board's file system
flash regions into one address space, and `pfs_fs.c` adapts PFS to the
subsystem. `include/pbl/fs/pfs.h` has what is specific to PFS:

- Files have a fixed size, so a new file gets its size from
  `pbl_fs_truncate()` right after `PBL_FS_O_CREATE` (or `PBL_FS_O_REPLACE`).
  Nothing reaches the flash before that, but the name is taken: opening it
  again fails with `-EBUSY`.
- There are no directories. Names may contain `/`, such as the
  `@<app id>/<suffix>` app files, and only the root of the mount can be
  listed.
- A file is open once at a time, and at most 8 are open at once.
- `PBL_FS_O_PFS_SKIP_HDR_CRC_CHECK` and `PBL_FS_O_PFS_USE_PAGE_CACHE` are
  hints for files opened often or read at random.
- `pbl_fs_pfs_optimal_size()` rounds a size up to use the whole flash pages a
  file takes anyway.

Names on the flash are relative to the mount point, so the mount point is not
part of the on-flash format.

## Shell

With `CONFIG_SHELL`, the `fs` command lists directories (`fs ls /`), shows
and removes files (`fs stat`, `fs rm`), dumps their contents (`fs read`) and
shows the space of a file system (`fs df`). The `pfs` command keeps the
PFS-specific tools, such as `pfs ls` with page and erase-count details.
