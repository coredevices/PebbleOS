/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "../fs_priv.h"
#include "pfs.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/util/math.h>

#include <kernel/pbl_malloc.h>

static bool s_mounted;

static int prv_errno(int status) {
  switch (status) {
    case E_INVALID_ARGUMENT:
      return -EINVAL;
    case E_DOES_NOT_EXIST:
      return -ENOENT;
    case E_BUSY:
      return -EBUSY;
    case E_OUT_OF_RESOURCES:
      return -EMFILE;
    case E_OUT_OF_STORAGE:
      return -ENOSPC;
    case E_OUT_OF_MEMORY:
      return -ENOMEM;
    default:
      return -EIO;
  }
}

static int prv_status(status_t status) {
  return (status >= 0) ? 0 : prv_errno(status);
}

static int prv_check_name(const char *name) {
  size_t len = strlen(name);

  if (len == 0) {
    return -EINVAL;
  }

  if (len > FILE_MAX_NAME_LEN) {
    return -ENAMETOOLONG;
  }

  return 0;
}

static int prv_fd(const struct pbl_fs_file *file) {
  return (int)(uintptr_t)file->filep;
}

static uint8_t prv_op_flags(uint32_t flags) {
  uint8_t op_flags = OP_FLAG_READ;

  if ((flags & PBL_FS_O_WRITE) != 0) {
    op_flags |= OP_FLAG_WRITE;
  }

  if ((flags & PBL_FS_O_PFS_SKIP_HDR_CRC_CHECK) != 0) {
    op_flags |= OP_FLAG_SKIP_HDR_CRC_CHECK;
  }

  if ((flags & PBL_FS_O_PFS_USE_PAGE_CACHE) != 0) {
    op_flags |= OP_FLAG_USE_PAGE_CACHE;
  }

  return op_flags;
}

static int prv_open(struct pbl_fs_file *file, const char *name, uint32_t flags) {
  int ret = prv_check_name(name);
  if (ret < 0) {
    return ret;
  }

  // Files are created with their size, by pbl_fs_truncate()
  uint8_t op_flags = prv_op_flags(flags);
  if ((flags & PBL_FS_O_CREATE) != 0) {
    op_flags |= OP_FLAG_WRITE | OP_FLAG_DEFER_CREATE;
  } else {
    op_flags |= OP_FLAG_NO_CREATE;
  }

  int fd;
  if ((flags & PBL_FS_O_REPLACE) != 0) {
    fd = pfs_open(name, (op_flags & ~OP_FLAG_WRITE) | OP_FLAG_OVERWRITE | OP_FLAG_DEFER_CREATE,
                  FILE_TYPE_STATIC, 0);
    if ((fd == E_DOES_NOT_EXIST) && ((flags & PBL_FS_O_CREATE) != 0)) {
      fd = pfs_open(name, op_flags, FILE_TYPE_STATIC, 0);
    }
  } else {
    fd = pfs_open(name, op_flags, FILE_TYPE_STATIC, 0);
  }

  if (fd < 0) {
    return prv_errno(fd);
  }

  file->filep = (void *)(uintptr_t)fd;
  file->name = pfs_get_file_name(fd);

  return 0;
}

static ssize_t prv_read(struct pbl_fs_file *file, void *buf, size_t len) {
  if (len == 0) {
    return 0;
  }

  int fd = prv_fd(file);
  int offset = pfs_seek(fd, 0, FSeekCur);
  if (offset < 0) {
    return prv_errno(offset);
  }

  len = MIN(len, pfs_get_file_size(fd) - (size_t)offset);
  if (len == 0) {
    return 0;
  }

  int ret = pfs_read(fd, buf, len);
  if (ret < 0) {
    return prv_errno(ret);
  }

  return ret;
}

static ssize_t prv_write(struct pbl_fs_file *file, const void *buf, size_t len) {
  if (len == 0) {
    return 0;
  }

  int ret = pfs_write(prv_fd(file), buf, len);
  if (ret == E_RANGE) {
    return -EFBIG;
  }

  if (ret < 0) {
    return prv_errno(ret);
  }

  return ret;
}

static off_t prv_lseek(struct pbl_fs_file *file, off_t offset, int whence) {
  int fd = prv_fd(file);
  int ret;

  switch (whence) {
    case PBL_FS_SEEK_SET:
      ret = pfs_seek(fd, offset, FSeekSet);
      break;
    case PBL_FS_SEEK_CUR:
      ret = pfs_seek(fd, offset, FSeekCur);
      break;
    default:
      ret = pfs_seek(fd, (off_t)pfs_get_file_size(fd) + offset, FSeekSet);
      break;
  }

  if (ret == E_RANGE) {
    return -EINVAL;
  }

  if (ret < 0) {
    return prv_errno(ret);
  }

  return ret;
}

static off_t prv_tell(struct pbl_fs_file *file) {
  return prv_lseek(file, 0, PBL_FS_SEEK_CUR);
}

static off_t prv_size(struct pbl_fs_file *file) {
  return pfs_get_file_size(prv_fd(file));
}

static int prv_truncate(struct pbl_fs_file *file, off_t length) {
  int fd = prv_fd(file);
  size_t size = pfs_get_file_size(fd);

  // Only files not created yet are empty
  if (size != 0) {
    return (length == (off_t)size) ? 0 : -ENOTSUP;
  }

  if (length == 0) {
    return 0;
  }

  return prv_status(pfs_create_deferred(fd, length));
}

static int prv_close(struct pbl_fs_file *file) {
  return prv_status(pfs_close(prv_fd(file)));
}

static int prv_opendir(struct pbl_fs_dir *dir, const char *name) {
  if (*name != '\0') {
    return -ENOENT;
  }

  dir->dirp = pfs_create_file_list(nullptr);

  return 0;
}

static int prv_readdir(struct pbl_fs_dir *dir, struct pbl_fs_dirent *entry) {
  PFSFileListEntry *head = dir->dirp;

  *entry = (struct pbl_fs_dirent){.type = PBL_FS_DIR_ENTRY_FILE};
  if (head == nullptr) {
    return 0;
  }

  entry->size = head->size;
  strncpy(entry->name, head->name, sizeof(entry->name) - 1);

  dir->dirp = list_get_next(&head->list_node);
  kernel_free(head);

  return 0;
}

static int prv_closedir(struct pbl_fs_dir *dir) {
  pfs_delete_file_list(dir->dirp);

  return 0;
}

static int prv_mount(struct pbl_fs_mount *mp) {
  if (s_mounted) {
    return -EBUSY;
  }

  int ret = prv_status(pfs_init(true));
  if (ret == 0) {
    s_mounted = true;
  }

  return ret;
}

static int prv_unmount(struct pbl_fs_mount *mp) {
  s_mounted = false;

  return 0;
}

static int prv_format(struct pbl_fs_mount *mp) {
  pfs_format(false);

  return 0;
}

static int prv_unlink(struct pbl_fs_mount *mp, const char *name) {
  int ret = prv_check_name(name);
  if (ret < 0) {
    return ret;
  }

  return prv_status(pfs_remove(name));
}

static int prv_stat(struct pbl_fs_mount *mp, const char *name, struct pbl_fs_dirent *entry) {
  int ret = prv_check_name(name);
  if (ret < 0) {
    return ret;
  }

  size_t size;
  ret = prv_status(pfs_stat(name, &size));
  if (ret < 0) {
    return ret;
  }

  *entry = (struct pbl_fs_dirent){.type = PBL_FS_DIR_ENTRY_FILE, .size = size};
  strncpy(entry->name, name, sizeof(entry->name) - 1);

  return 0;
}

static int prv_statvfs(struct pbl_fs_mount *mp, const char *name, struct pbl_fs_statvfs *stat) {
  *stat = (struct pbl_fs_statvfs){
    .f_bsize = 1,
    .f_frsize = 1,
    .f_blocks = pfs_get_size(),
    .f_bfree = get_available_pfs_space(),
  };

  return 0;
}

const struct pbl_fs_ops pbl_fs_pfs_ops = {
  .open = prv_open,
  .read = prv_read,
  .write = prv_write,
  .lseek = prv_lseek,
  .tell = prv_tell,
  .size = prv_size,
  .truncate = prv_truncate,
  .close = prv_close,
  .opendir = prv_opendir,
  .readdir = prv_readdir,
  .closedir = prv_closedir,
  .mount = prv_mount,
  .unmount = prv_unmount,
  .format = prv_format,
  .unlink = prv_unlink,
  .stat = prv_stat,
  .statvfs = prv_statvfs,
};

int pbl_fs_pfs_optimal_size(const char *path, size_t min_size) {
  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(path, &name);
  if ((mp == nullptr) || (mp->ops != &pbl_fs_pfs_ops)) {
    return -EINVAL;
  }

  return pfs_sector_optimal_size(min_size, strlen(name));
}
