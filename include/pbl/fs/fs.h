/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/fs/fs_sys.h>
#include <pbl/util/bits.h>

#include <sys/types.h>

/**
 * @defgroup fs File system
 * @ingroup subsys
 * @brief Access to files regardless of the file system format.
 *
 * File systems are mounted at absolute paths, such as the root @c "/" or @c "/sdcard", and
 * files are reached through paths below them, such as @c "/pindb". A path belongs to the mount
 * with the longest matching mount point. The format behind a mount point is a backend
 * (struct pbl_fs_ops); formats differ in what they support:
 *
 * - Some formats have fixed-size files: a file then gets its size from pbl_fs_truncate() once,
 *   right after it is created, and reads and writes stay within it.
 * - Some formats have no directories, and their file names may contain @c '/'.
 *
 * Every function returns 0 (or a count or offset) on success and a negative errno on failure,
 * @c -ENOTSUP for what the format does not support. Files and directories are handles owned by
 * the caller; a handle is used by one thread at a time.
 *
 * @code{.c}
 * struct pbl_fs_file f;
 * int ret = pbl_fs_open(&f, "/myfile", PBL_FS_O_RDWR | PBL_FS_O_CREATE);
 * if (ret == 0) {
 *   if (pbl_fs_size(&f) == 0) {
 *     ret = pbl_fs_truncate(&f, 128);
 *   }
 *   if (ret == 0) {
 *     ret = pbl_fs_write(&f, data, sizeof(data));
 *   }
 *   pbl_fs_close(&f);
 * }
 * @endcode
 * @{
 */

/** @brief Maximum length of a file name, without the NUL terminator. */
#define PBL_FS_NAME_MAX 255

/** @brief Open for reading. */
#define PBL_FS_O_READ PBL_BIT(0)
/** @brief Open for writing. */
#define PBL_FS_O_WRITE PBL_BIT(1)
/** @brief Open for reading and writing. */
#define PBL_FS_O_RDWR (PBL_FS_O_READ | PBL_FS_O_WRITE)
/** @brief Create the file, empty, if it does not exist. */
#define PBL_FS_O_CREATE PBL_BIT(2)
/**
 * @brief Write a new, empty version of the file that replaces the current one on close.
 *
 * Until pbl_fs_close(), opening the path returns the current version, and a reboot keeps it.
 * Fails with @c -ENOENT if the file does not exist, unless @ref PBL_FS_O_CREATE is set, which
 * then creates it as usual. Implies @ref PBL_FS_O_WRITE.
 */
#define PBL_FS_O_REPLACE PBL_BIT(3)
/**
 * @brief Format-specific open flag @p n, from 0 to 15.
 *
 * Hints that formats without the flag ignore. The formats define their own names for them.
 */
#define PBL_FS_O_FORMAT(n) PBL_BIT(16 + (n))

/** @brief pbl_fs_seek() from the start of the file. */
#define PBL_FS_SEEK_SET 0
/** @brief pbl_fs_seek() from the current position. */
#define PBL_FS_SEEK_CUR 1
/** @brief pbl_fs_seek() from the end of the file. */
#define PBL_FS_SEEK_END 2

/** @brief Type of a directory entry. */
enum pbl_fs_dir_entry_type {
  /** Regular file. */
  PBL_FS_DIR_ENTRY_FILE,
  /** Directory. */
  PBL_FS_DIR_ENTRY_DIR,
};

/** @brief Directory entry, see pbl_fs_readdir() and pbl_fs_stat(). */
struct pbl_fs_dirent {
  /** Type of the entry. */
  enum pbl_fs_dir_entry_type type;
  /** Size of a file in bytes. */
  size_t size;
  /** NUL-terminated name, relative to the directory. */
  char name[PBL_FS_NAME_MAX + 1];
};

/** @brief Space of a file system, see pbl_fs_statvfs(). */
struct pbl_fs_statvfs {
  /** Optimal transfer size in bytes. */
  unsigned long f_bsize;
  /** Allocation unit in bytes. */
  unsigned long f_frsize;
  /** Size of the file system in @c f_frsize units. */
  unsigned long f_blocks;
  /** Space available for new data in @c f_frsize units. */
  unsigned long f_bfree;
};

/** @brief An open file, see pbl_fs_open(). */
struct pbl_fs_file {
  /** Private to the backend. */
  void *filep;
  /** Set by the backend on open: name relative to the mount point, valid until closed. */
  const char *name;
  /** Private to the core. */
  struct pbl_fs_mount *mp;
  /** Private to the core. */
  uint32_t flags;
};

/** @brief An open directory, see pbl_fs_opendir(). */
struct pbl_fs_dir {
  /** Private to the backend. */
  void *dirp;
  /** Private to the core. */
  struct pbl_fs_mount *mp;
};

/** @brief pbl_fs_watch_add() event: a file opened for writing was closed. */
#define PBL_FS_WATCH_CLOSED PBL_BIT(0)
/** @brief pbl_fs_watch_add() event: the file was removed. */
#define PBL_FS_WATCH_REMOVED PBL_BIT(1)
/** @brief pbl_fs_watch_add() events: all of them. */
#define PBL_FS_WATCH_ALL (PBL_FS_WATCH_CLOSED | PBL_FS_WATCH_REMOVED)

/**
 * @brief Callback of pbl_fs_watch_add().
 *
 * Runs on the thread that closed or removed the file. It must not add or remove watches.
 *
 * @param user_data Data passed to pbl_fs_watch_add().
 */
typedef void (*pbl_fs_watch_cb_t)(void *user_data);

/** @brief A file watch, see pbl_fs_watch_add(). */
struct pbl_fs_watch;

/**
 * @brief Mount a file system.
 *
 * @param mp Mount, with its mount point and format set.
 * @retval -EBUSY The mount point is in use.
 * @retval -EINVAL Invalid mount point.
 * @return Other negative errno from the backend.
 */
int pbl_fs_mount(struct pbl_fs_mount *mp);

/**
 * @brief Unmount a file system.
 *
 * @param mp Mount passed to pbl_fs_mount().
 */
int pbl_fs_unmount(struct pbl_fs_mount *mp);

/**
 * @brief Erase every file of a mounted file system.
 *
 * Files open on it become invalid; they still have to be closed.
 *
 * @param mnt_point Mount point.
 * @retval -ENOENT Nothing is mounted at @p mnt_point.
 */
int pbl_fs_format(const char *mnt_point);

/**
 * @brief Open a file.
 *
 * @param file Handle to initialize.
 * @param path Absolute path.
 * @param flags @c PBL_FS_O_* flags.
 * @retval -ENOENT The file does not exist and is not to be created, or nothing is mounted there.
 * @retval -EBUSY The file is already open, on formats that open a file only once.
 * @retval -EMFILE Too many open files.
 * @retval -ENOSPC Not enough space.
 * @return Other negative errno on failure.
 */
int pbl_fs_open(struct pbl_fs_file *file, const char *path, uint32_t flags);

/**
 * @brief Read from the current position, then advance it.
 *
 * @param file Open file.
 * @param buf Destination buffer.
 * @param len Number of bytes to read.
 * @return Number of bytes read, less than @p len at the end of the file, or negative errno.
 */
ssize_t pbl_fs_read(struct pbl_fs_file *file, void *buf, size_t len);

/**
 * @brief Write at the current position, then advance it.
 *
 * @param file File opened for writing.
 * @param buf Data to write.
 * @param len Number of bytes to write.
 * @return Number of bytes written, or negative errno.
 * @retval -EFBIG The write goes past the end of a fixed-size file.
 */
ssize_t pbl_fs_write(struct pbl_fs_file *file, const void *buf, size_t len);

/**
 * @brief Set the current position.
 *
 * @param file Open file.
 * @param offset Offset from @p whence.
 * @param whence @ref PBL_FS_SEEK_SET, @ref PBL_FS_SEEK_CUR or @ref PBL_FS_SEEK_END.
 * @return New position, or negative errno.
 * @retval -EINVAL The position is outside the file, on formats with fixed-size files.
 */
off_t pbl_fs_seek(struct pbl_fs_file *file, off_t offset, int whence);

/**
 * @brief Get the current position.
 *
 * @param file Open file.
 * @return Current position, or negative errno.
 */
off_t pbl_fs_tell(struct pbl_fs_file *file);

/**
 * @brief Get the size of an open file.
 *
 * @param file Open file.
 * @return Size in bytes, or negative errno.
 */
off_t pbl_fs_size(struct pbl_fs_file *file);

/**
 * @brief Set the size of a file.
 *
 * On formats with fixed-size files, sets the size of a file that is still empty, after
 * @ref PBL_FS_O_CREATE or @ref PBL_FS_O_REPLACE; other sizes fail with @c -ENOTSUP. The new
 * space reads as erased flash there (@c 0xff), and as zeros on other formats.
 *
 * @param file File opened for writing.
 * @param length New size in bytes.
 * @retval -ENOSPC Not enough space.
 */
int pbl_fs_truncate(struct pbl_fs_file *file, off_t length);

/**
 * @brief Flush cached data of a file to the storage.
 *
 * @param file Open file.
 */
int pbl_fs_sync(struct pbl_fs_file *file);

/**
 * @brief Close a file.
 *
 * Commits a @ref PBL_FS_O_REPLACE version and notifies watchers if the file was open for
 * writing. The handle is invalid afterwards, even on failure.
 *
 * @param file Open file.
 */
int pbl_fs_close(struct pbl_fs_file *file);

/**
 * @brief Remove a file.
 *
 * @param path Absolute path.
 * @retval -ENOENT The file does not exist.
 */
int pbl_fs_unlink(const char *path);

/**
 * @brief Rename a file, replacing @p to if it exists.
 *
 * @param from Absolute path of the file.
 * @param to New absolute path, on the same file system.
 * @retval -EXDEV The paths are on different file systems.
 */
int pbl_fs_rename(const char *from, const char *to);

/**
 * @brief Create a directory.
 *
 * @param path Absolute path.
 */
int pbl_fs_mkdir(const char *path);

/**
 * @brief Get the type and size of a file or directory.
 *
 * @param path Absolute path.
 * @param[out] entry Type, size and name (the last component of @p path).
 * @retval -ENOENT It does not exist.
 */
int pbl_fs_stat(const char *path, struct pbl_fs_dirent *entry);

/**
 * @brief Get the space of the file system holding a path.
 *
 * @param path Absolute path.
 * @param[out] stat Space.
 */
int pbl_fs_statvfs(const char *path, struct pbl_fs_statvfs *stat);

/**
 * @brief Open a directory.
 *
 * @param dir Handle to initialize.
 * @param path Absolute path, such as a mount point.
 */
int pbl_fs_opendir(struct pbl_fs_dir *dir, const char *path);

/**
 * @brief Read the next entry of a directory.
 *
 * Entries may come in any order. Removing a file while reading does not change the entries
 * still to come.
 *
 * @param dir Open directory.
 * @param[out] entry Next entry, with an empty name once there are no more.
 */
int pbl_fs_readdir(struct pbl_fs_dir *dir, struct pbl_fs_dirent *entry);

/**
 * @brief Close a directory.
 *
 * @param dir Open directory.
 */
int pbl_fs_closedir(struct pbl_fs_dir *dir);

/**
 * @brief Watch a file for changes.
 *
 * @param path Absolute path of the file, which need not exist.
 * @param events @c PBL_FS_WATCH_* events that run @p cb.
 * @param cb Callback.
 * @param user_data Data passed to @p cb.
 * @return Watch, for pbl_fs_watch_remove(). Never NULL.
 */
struct pbl_fs_watch *pbl_fs_watch_add(const char *path, uint32_t events, pbl_fs_watch_cb_t cb,
                                      void *user_data);

/**
 * @brief Stop watching a file.
 *
 * @param watch Watch returned by pbl_fs_watch_add().
 */
void pbl_fs_watch_remove(struct pbl_fs_watch *watch);

/** @} */
