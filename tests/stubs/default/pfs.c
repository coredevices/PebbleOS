/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/filesystem/pfs.h>

PBL_WEAK status_t pfs_remove(const char *name) {
  return S_SUCCESS;
}

PBL_WEAK int pfs_seek(int fd, int offset, FSeekType seek_type) {
  return 0;
}

PBL_WEAK int pfs_read(int fd, void *buf_ptr, size_t size) {
  return 0;
}
