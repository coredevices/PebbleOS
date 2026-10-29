/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK int pbl_fs_unlink(const char *path) {
  return 0;
}

PBL_WEAK off_t pbl_fs_seek(struct pbl_fs_file *file, off_t offset, int whence) {
  return 0;
}

PBL_WEAK ssize_t pbl_fs_read(struct pbl_fs_file *file, void *buf, size_t len) {
  return 0;
}
