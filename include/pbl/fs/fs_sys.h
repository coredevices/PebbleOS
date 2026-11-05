/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/util/list.h>

#include <sys/types.h>

/**
 * @defgroup fs_sys File system backends
 * @ingroup fs
 * @brief Interface between the file system core and the file system formats.
 *
 * A format implements struct pbl_fs_ops. The core resolves paths to a mount and hands the
 * backend the path relative to the mount point, without the leading @c '/' (@c "" for the root
 * of the mount). The core serializes nothing: backends are responsible for their own locking.
 * @{
 */

struct pbl_fs_mount;
struct pbl_fs_file;
struct pbl_fs_dir;
struct pbl_fs_dirent;
struct pbl_fs_statvfs;

/**
 * @brief Operations of a file system format.
 *
 * Every operation returns 0 (or a count or offset) on success and a negative errno on failure.
 * Operations a format does not support may be NULL; the core returns @c -ENOTSUP for them.
 */
struct pbl_fs_ops {
  /** Open @p name with @c PBL_FS_O_* @p flags; set struct pbl_fs_file::filep and name. */
  int (*open)(struct pbl_fs_file *file, const char *name, uint32_t flags);
  /** Read up to @p len bytes at the current position. */
  ssize_t (*read)(struct pbl_fs_file *file, void *buf, size_t len);
  /** Write @p len bytes at the current position. */
  ssize_t (*write)(struct pbl_fs_file *file, const void *buf, size_t len);
  /** Move the current position. */
  off_t (*lseek)(struct pbl_fs_file *file, off_t offset, int whence);
  /** Get the current position. */
  off_t (*tell)(struct pbl_fs_file *file);
  /** Get the size of the file. */
  off_t (*size)(struct pbl_fs_file *file);
  /** Set the size of the file. */
  int (*truncate)(struct pbl_fs_file *file, off_t length);
  /** Flush cached data to the storage. */
  int (*sync)(struct pbl_fs_file *file);
  /** Close the file. */
  int (*close)(struct pbl_fs_file *file);
  /** Open directory @p name; set struct pbl_fs_dir::dirp. */
  int (*opendir)(struct pbl_fs_dir *dir, const char *name);
  /** Read the next entry; an empty name marks the end. */
  int (*readdir)(struct pbl_fs_dir *dir, struct pbl_fs_dirent *entry);
  /** Close the directory. */
  int (*closedir)(struct pbl_fs_dir *dir);
  /** Mount the file system. */
  int (*mount)(struct pbl_fs_mount *mp);
  /** Unmount the file system. */
  int (*unmount)(struct pbl_fs_mount *mp);
  /** Erase the file system, invalidating every open file. */
  int (*format)(struct pbl_fs_mount *mp);
  /** Remove a file. */
  int (*unlink)(struct pbl_fs_mount *mp, const char *name);
  /** Rename a file. */
  int (*rename)(struct pbl_fs_mount *mp, const char *from, const char *to);
  /** Create a directory. */
  int (*mkdir)(struct pbl_fs_mount *mp, const char *name);
  /** Get the type and size of a file. */
  int (*stat)(struct pbl_fs_mount *mp, const char *name, struct pbl_fs_dirent *entry);
  /** Get the space of the file system. */
  int (*statvfs)(struct pbl_fs_mount *mp, const char *name, struct pbl_fs_statvfs *stat);
};

/**
 * @brief A mounted file system.
 *
 * Owned by the caller of pbl_fs_mount() for as long as it stays mounted.
 */
struct pbl_fs_mount {
  /** Mount point: @c "/", or an absolute path without a trailing @c '/', such as @c "/sdcard". */
  const char *mnt_point;
  /** Format of the file system. */
  const struct pbl_fs_ops *ops;
  /** Optional. Data of the format, such as the storage to use. */
  void *fs_data;
  /** Set by the core. */
  ListNode node;
  /** Set by the core. */
  size_t mnt_point_len;
};

/** @} */
