/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <pbl/fs/fs.h>

/**
 * @defgroup fs_pfs Pebble File System
 * @ingroup fs
 * @brief PFS, the wear-leveled file system on the external NOR flash.
 *
 * - Files have a fixed size, set with pbl_fs_truncate() right after they are created.
 * - There are no directories: file names may contain @c '/', and only the root of the mount can
 *   be listed.
 * - A file is open once at a time, and at most 8 files are open at once.
 * - The flash is NOR: a bit written to 0 stays 0 until the file is rewritten, which
 *   @ref PBL_FS_O_REPLACE does safely.
 * - Erasing flash is slow and costs power: avoid deleting and creating files repeatedly.
 * - Removing a file that is open is a fatal error.
 * - Only one PFS can be mounted; the normal firmware mounts it at the root, @c "/".
 * @{
 */

/** @brief Skip checking the on-flash header CRCs, for files opened thousands of times. */
#define PBL_FS_O_PFS_SKIP_HDR_CRC_CHECK PBL_FS_O_FORMAT(0)
/**
 * @brief Cache the translation from file pages to flash pages.
 *
 * Speeds up random access to large files. Best limited to reads, so that heap corruption cannot
 * corrupt the file.
 */
#define PBL_FS_O_PFS_USE_PAGE_CACHE PBL_FS_O_FORMAT(1)

/** @brief PFS format, for struct pbl_fs_mount::ops. */
extern const struct pbl_fs_ops pbl_fs_pfs_ops;

/**
 * @brief Get the optimal size of a file that can use space beyond a minimum.
 *
 * Rounds @p min_size up to use all the space of the flash pages the file takes anyway.
 *
 * @param path Absolute path of the file.
 * @param min_size Minimum size in bytes.
 * @return Optimal size in bytes, or negative errno if @p path is not on a PFS mount.
 */
int pbl_fs_pfs_optimal_size(const char *path, size_t min_size);

/** @} */
