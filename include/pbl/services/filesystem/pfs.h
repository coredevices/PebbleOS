/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stddef.h>

#include "kernel/pebble_tasks.h"
#include "system/status_codes.h"
#include "pbl/util/list.h"

/**
 * @defgroup services_filesystem Filesystem
 * @ingroup services
 * @brief Pebble File System (PFS) on the external NOR flash.
 *
 * - All functions are thread-safe.
 * - PFS implements basic wear leveling to extend the life of the flash part.
 * - A file is a block of space of fixed size, set when it is created, that appears to the
 *   caller as a contiguous region. Reads and writes past the end of the file fail. How the
 *   space is used is up to the caller.
 * - The flash is NOR: once a bit is written to 0, the file must be erased or rewritten to set it
 *   back to 1. Opening with @ref OP_FLAG_OVERWRITE provides a safe way to do this.
 * - Erasing flash sectors is slow and costs power: avoid repeatedly deleting and creating files.
 *
 * @code{.c}
 * int fd = pfs_open("myfile", OP_FLAG_WRITE | OP_FLAG_READ, FILE_TYPE_STATIC, 128);
 * if (fd >= 0) {
 *   pfs_write(fd, data, sizeof(data));
 *   pfs_seek(fd, 0, FSeekSet);
 *   pfs_read(fd, buf, sizeof(data));
 *   pfs_close(fd);
 * }
 * @endcode
 * @{
 */

/** @brief Open for reading; fails if the file does not exist. */
#define OP_FLAG_READ (1 << 0)
/** @brief Open for writing, creating the file if it does not exist. */
#define OP_FLAG_WRITE (1 << 1)
/** @brief Write a new version of an existing file, committed on pfs_close(). */
#define OP_FLAG_OVERWRITE (1 << 2)
/** @brief Skip checking the on-flash header CRCs. */
#define OP_FLAG_SKIP_HDR_CRC_CHECK (1 << 3)
/** @brief Cache the translation from file pages to flash pages. */
#define OP_FLAG_USE_PAGE_CACHE (1 << 4)

/** @brief File type of regular files. */
#define FILE_TYPE_STATIC (0xfe)
/** @brief Maximum length of a file name, without the NUL terminator. */
#define FILE_MAX_NAME_LEN (255)

/** @brief Reference point of pfs_seek(). */
typedef enum {
  /** Offset from the start of the file. */
  FSeekSet,
  /** Offset from the current position. */
  FSeekCur
} FSeekType;

/** @brief pfs_watch_file() event: the file was closed after being opened for writing. */
#define FILE_CHANGED_EVENT_CLOSED (1 << 0)
/** @brief pfs_watch_file() event: the file was removed. */
#define FILE_CHANGED_EVENT_REMOVED (1 << 1)
/** @brief pfs_watch_file() events: all of them. */
#define FILE_CHANGED_EVENT_ALL (FILE_CHANGED_EVENT_CLOSED | FILE_CHANGED_EVENT_REMOVED)

/**
 * @brief Callback of pfs_watch_file().
 *
 * Runs on the task that closed or removed the file, with the PFS lock held: it must not call
 * PFS.
 *
 * @param data Data passed to pfs_watch_file().
 */
typedef void (*PFSFileChangedCallback)(void *data);

/** @brief Handle of a file watch, for pfs_unwatch_file(). */
typedef void *PFSCallbackHandle;

/**
 * @brief File name filter of pfs_create_file_list() and pfs_remove_files().
 *
 * @param name File name.
 * @return true if the file matches.
 */
typedef bool (*PFSFilenameTestCallback)(const char *name);

/** @brief Entry of the list returned by pfs_create_file_list(). */
typedef struct {
  /** List node. */
  ListNode list_node;
  /** NUL-terminated file name. */
  char name[];
} PFSFileListEntry;

/**
 * @brief Open a file.
 *
 * Flags:
 * - @ref OP_FLAG_READ - pfs_read() works. With only this flag, fails if the file does not exist.
 * - @ref OP_FLAG_WRITE - creates the file if it does not exist; pfs_write() works. The caller
 *   seeks to the desired offset.
 * - @ref OP_FLAG_OVERWRITE - safely and incrementally overwrites an existing file; fails if the
 *   file does not exist. The new version is committed by pfs_close(); until then, opening
 *   @p name returns the original file. There is always a valid version to read, and the caller
 *   can copy parts of the original file in chunks instead of allocating a lot of RAM.
 * - @ref OP_FLAG_SKIP_HDR_CRC_CHECK - skips the sanity check of the on-flash header CRCs, worth
 *   it for files opened thousands of times.
 * - @ref OP_FLAG_USE_PAGE_CACHE - caches the translation from file pages to flash pages, which
 *   speeds up random access to large files. Best limited to reads, so that heap corruption
 *   cannot corrupt the file.
 *
 * @param name File name, 1 to @ref FILE_MAX_NAME_LEN characters.
 * @param op_flags @c OP_FLAG_* flags.
 * @param file_type File type, used only when the file is created or overwritten.
 * @param start_size File size in bytes, used only when the file is created or overwritten.
 * @return File descriptor (>= 0) on success, negative @c status_t on failure.
 * @retval E_INVALID_ARGUMENT Invalid name, or invalid type or zero size on creation.
 * @retval E_DOES_NOT_EXIST The file does not exist and was not to be created.
 * @retval E_BUSY The file is already open.
 * @retval E_OUT_OF_RESOURCES No free file descriptor.
 * @retval E_OUT_OF_STORAGE Not enough space to create the file.
 */
extern int pfs_open(const char *name, uint8_t op_flags, uint8_t file_type, size_t start_size);

/**
 * @brief Write at the current position, then advance it.
 *
 * @param fd File descriptor opened for writing or overwriting.
 * @param buf Data to write.
 * @param size Number of bytes to write, at most the size of @p buf.
 * @return Number of bytes written, or negative @c status_t on failure.
 * @retval E_INVALID_ARGUMENT Invalid descriptor, not writable, or empty buffer.
 * @retval E_RANGE The write goes past the end of the file.
 */
extern int pfs_write(int fd, const void *buf, size_t size);

/**
 * @brief Read from the current position, then advance it.
 *
 * @param fd File descriptor opened for reading.
 * @param[out] buf Destination buffer.
 * @param size Number of bytes to read, at most the size of @p buf.
 * @return Number of bytes read, or negative @c status_t on failure.
 * @retval E_INVALID_ARGUMENT Invalid descriptor, not readable, or empty buffer.
 * @retval E_RANGE The read goes past the end of the file.
 */
extern int pfs_read(int fd, void *buf, size_t size);

/**
 * @brief Set the current position.
 *
 * @param fd File descriptor.
 * @param offset Offset relative to @p seek_type.
 * @param seek_type Reference point.
 * @return New position on success, negative @c status_t on failure.
 * @retval E_INVALID_ARGUMENT Invalid descriptor.
 * @retval E_RANGE Position outside 0 to the file size.
 */
extern int pfs_seek(int fd, int offset, FSeekType seek_type);

/**
 * @brief Close a file.
 *
 * Commits an overwrite and notifies watchers if the file was opened for writing.
 *
 * @param fd File descriptor.
 * @retval S_SUCCESS File closed.
 * @retval E_INVALID_ARGUMENT Invalid descriptor.
 */
extern status_t pfs_close(int fd);

/**
 * @brief Close and remove a file.
 *
 * @param fd File descriptor.
 * @return @c S_SUCCESS or negative @c status_t, as pfs_close() and pfs_remove().
 */
extern status_t pfs_close_and_remove(int fd);

/**
 * @brief Remove a file.
 *
 * @param name File name.
 * @retval S_SUCCESS File removed.
 * @retval E_INVALID_ARGUMENT Invalid name.
 * @return Other negative @c status_t on failure.
 */
extern status_t pfs_remove(const char *name);

/**
 * @brief Get the size of a file, that is the number of bytes that can be read.
 *
 * @param fd File descriptor.
 * @return Size in bytes, 0 for an invalid descriptor.
 */
extern size_t pfs_get_file_size(int fd);

/**
 * @brief Initialize PFS, before any other use.
 *
 * Builds the flash translation layer, recovers from an interrupted garbage collection, and
 * pre-erases some space.
 *
 * @param run_filesystem_check Format the flash if PFS is not active on it.
 * @retval S_SUCCESS Always.
 */
extern status_t pfs_init(bool run_filesystem_check);

/**
 * @brief Clean up after a reboot, once and before any file operation.
 *
 * Finishes or rolls back operations interrupted by the reboot.
 */
extern void pfs_reboot_cleanup(void);

/**
 * @brief Erase the whole filesystem and drop all open file descriptors.
 *
 * Requires pfs_init() to have been called.
 *
 * @param write_erase_headers Mark all pages as erased.
 */
extern void pfs_format(bool write_erase_headers);

/**
 * @brief Get the size of the filesystem.
 *
 * @return Size in bytes.
 */
extern uint32_t pfs_get_size(void);

/**
 * @brief Set the size of the filesystem, as regions are added to it.
 *
 * @param new_size New size in bytes.
 * @param new_region_erased The added pages are erased and should be marked as such.
 */
extern void pfs_set_size(uint32_t new_size, bool new_region_erased);

/**
 * @brief Check whether PFS is active on this device.
 *
 * @return true if PFS is active.
 */
extern bool pfs_active(void);

/**
 * @brief Check whether PFS is active in a range of the filesystem space.
 *
 * @param start_address Start of the range.
 * @param ending_address End of the range, exclusive.
 * @return true if PFS is active in the range.
 */
extern bool pfs_active_in_region(uint32_t start_address, uint32_t ending_address);

/**
 * @brief Get the optimal size of a file that can use space beyond a minimum.
 *
 * Rounds @p min_size up to use all the space of the pages the file occupies anyway.
 *
 * @param min_size Minimum file size in bytes.
 * @param namelen Length of the file name.
 * @return Optimal file size in bytes.
 */
extern int pfs_sector_optimal_size(int min_size, int namelen);

/**
 * @brief Get the space available for new files.
 *
 * Only 80% of the filesystem is considered usable, to leave room for wear leveling.
 *
 * @return Available space in bytes.
 */
extern uint32_t get_available_pfs_space(void);

/**
 * @brief Watch a file for changes.
 *
 * @param filename Name of the file to watch.
 * @param callback Function called on the selected events.
 * @param event_flags @c FILE_CHANGED_EVENT_* flags selecting the events.
 * @param data Pointer passed to @p callback.
 * @return Handle for pfs_unwatch_file().
 */
PFSCallbackHandle pfs_watch_file(const char *filename, PFSFileChangedCallback callback,
                                 uint8_t event_flags, void *data);

/**
 * @brief Stop watching a file.
 *
 * @param cb_handle Handle returned by pfs_watch_file().
 */
void pfs_unwatch_file(PFSCallbackHandle cb_handle);

/**
 * @brief Compute the legacy CRC-32 of a part of a file.
 *
 * Moves the current position of @p fd.
 *
 * @param fd File descriptor opened for reading.
 * @param offset Start offset.
 * @param num_bytes Number of bytes.
 * @return Checksum, see pbl_crc32_legacy().
 */
extern uint32_t pfs_crc_calculate_file(int fd, uint32_t offset, uint32_t num_bytes);

/**
 * @brief List the files whose name matches a filter.
 *
 * @param callback Name filter, or NULL to include all files.
 * @return Head of a list of matching names, or NULL if none match. Free it with
 * pfs_delete_file_list().
 */
extern PFSFileListEntry *pfs_create_file_list(PFSFilenameTestCallback callback);

/**
 * @brief Free a list returned by pfs_create_file_list().
 *
 * @param list Head of the list.
 */
extern void pfs_delete_file_list(PFSFileListEntry *list);

/**
 * @brief Remove all files whose name matches a filter.
 *
 * @param callback Name filter.
 */
extern void pfs_remove_files(PFSFilenameTestCallback callback);

/** @} */
