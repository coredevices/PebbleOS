/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_pfs.h"

#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/kernel/assert.h>

static struct pbl_fs_mount s_mount = {.mnt_point = "/", .ops = &pbl_fs_pfs_ops};

void fake_pfs_mount(void) {
  pbl_fs_unmount(&s_mount);
  int ret = pbl_fs_mount(&s_mount);
  PBL_ASSERTN(ret == 0);
}

int fake_pfs_create(struct pbl_fs_file *file, const char *path, size_t size) {
  int ret = pbl_fs_open(file, path, PBL_FS_O_RDWR | PBL_FS_O_CREATE);
  if ((ret == 0) && (pbl_fs_size(file) == 0)) {
    ret = pbl_fs_truncate(file, size);
    if (ret < 0) {
      pbl_fs_close(file);
    }
  }

  return ret;
}
