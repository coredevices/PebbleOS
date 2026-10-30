/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fs_priv.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/util/list.h>
#include <pbl/util/misc.h>

#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DEFINE(fs, CONFIG_FS_LOG_LEVEL);

struct pbl_fs_watch {
  ListNode node;
  char *path;
  uint32_t events;
  pbl_fs_watch_cb_t cb;
  void *user_data;
  bool pending;
};

static PBL_MUTEX_DEFINE(s_mounts_lock);
static ListNode *s_mounts;

static PBL_MUTEX_DEFINE(s_watches_lock);
static ListNode *s_watches;

struct pbl_fs_mount *pbl_fs__mount_get(const char *path, const char **name) {
  if ((path == nullptr) || (path[0] != '/')) {
    return nullptr;
  }

  struct pbl_fs_mount *best = nullptr;

  pbl_mutex_lock(&s_mounts_lock, PBL_FOREVER);
  for (ListNode *n = s_mounts; n != nullptr; n = list_get_next(n)) {
    struct pbl_fs_mount *mp = container_of(n, struct pbl_fs_mount, node);
    size_t len = mp->mnt_point_len;

    if ((strncmp(path, mp->mnt_point, len) != 0) || ((path[len] != '\0') && (path[len] != '/'))) {
      continue;
    }

    if ((best == nullptr) || (len > best->mnt_point_len)) {
      best = mp;
    }
  }
  pbl_mutex_unlock(&s_mounts_lock);

  if (best != nullptr) {
    *name = path + best->mnt_point_len;
    if (**name == '/') {
      (*name)++;
    }
  }

  return best;
}

static bool prv_path_is(const char *path, const struct pbl_fs_mount *mp, const char *name) {
  size_t len = mp->mnt_point_len;
  return (strncmp(path, mp->mnt_point, len) == 0) && (path[len] == '/') &&
         (strcmp(path + len + 1, name) == 0);
}

static void prv_watch_notify(const char *path, uint32_t event) {
  pbl_mutex_lock(&s_watches_lock, PBL_FOREVER);
  for (ListNode *n = s_watches; n != nullptr; n = list_get_next(n)) {
    struct pbl_fs_watch *w = container_of(n, struct pbl_fs_watch, node);

    if (((w->events & event) != 0) && (strcmp(w->path, path) == 0)) {
      w->cb(w->user_data);
    }
  }
  pbl_mutex_unlock(&s_watches_lock);
}

int pbl_fs_mount(struct pbl_fs_mount *mp) {
  const char *mnt_point = mp->mnt_point;
  size_t len = (mnt_point == nullptr) ? 0 : strlen(mnt_point);

  if ((len == 0) || (mnt_point[0] != '/') || ((len > 1) && (mnt_point[len - 1] == '/')) ||
      (mp->ops == nullptr)) {
    return -EINVAL;
  }

  pbl_mutex_lock(&s_mounts_lock, PBL_FOREVER);

  int ret = 0;
  for (ListNode *n = s_mounts; n != nullptr; n = list_get_next(n)) {
    struct pbl_fs_mount *other = container_of(n, struct pbl_fs_mount, node);

    if ((other == mp) || (strcmp(other->mnt_point, mnt_point) == 0)) {
      ret = -EBUSY;
      break;
    }
  }

  if ((ret == 0) && (mp->ops->mount != nullptr)) {
    ret = mp->ops->mount(mp);
  }

  if (ret == 0) {
    // The root matches every path through its leading '/'
    mp->mnt_point_len = (len == 1) ? 0 : len;
    list_init(&mp->node);
    s_mounts = list_prepend(s_mounts, &mp->node);
    PBL_LOG_DBG("Mounted %s", mnt_point);
  }

  pbl_mutex_unlock(&s_mounts_lock);

  return ret;
}

int pbl_fs_unmount(struct pbl_fs_mount *mp) {
  pbl_mutex_lock(&s_mounts_lock, PBL_FOREVER);

  int ret = 0;
  if (!list_contains(s_mounts, &mp->node)) {
    ret = -EINVAL;
  } else if (mp->ops->unmount != nullptr) {
    ret = mp->ops->unmount(mp);
  }

  if (ret == 0) {
    list_remove(&mp->node, &s_mounts, nullptr);
  }

  pbl_mutex_unlock(&s_mounts_lock);

  return ret;
}

int pbl_fs_format(const char *mnt_point) {
  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(mnt_point, &name);

  if ((mp == nullptr) || (*name != '\0')) {
    return -ENOENT;
  }

  if (mp->ops->format == nullptr) {
    return -ENOTSUP;
  }

  return mp->ops->format(mp);
}

int pbl_fs_open(struct pbl_fs_file *file, const char *path, uint32_t flags) {
  *file = (struct pbl_fs_file){};

  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(path, &name);
  if (mp == nullptr) {
    return -ENOENT;
  }

  if (*name == '\0') {
    return -EISDIR;
  }

  if (mp->ops->open == nullptr) {
    return -ENOTSUP;
  }

  if ((flags & PBL_FS_O_REPLACE) != 0) {
    flags |= PBL_FS_O_WRITE;
  }

  file->mp = mp;
  file->flags = flags;

  int ret = mp->ops->open(file, name, flags);
  if (ret < 0) {
    *file = (struct pbl_fs_file){};
  }

  return ret;
}

ssize_t pbl_fs_read(struct pbl_fs_file *file, void *buf, size_t len) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  if ((file->flags & PBL_FS_O_READ) == 0) {
    return -EACCES;
  }

  if (file->mp->ops->read == nullptr) {
    return -ENOTSUP;
  }

  return file->mp->ops->read(file, buf, len);
}

ssize_t pbl_fs_write(struct pbl_fs_file *file, const void *buf, size_t len) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  if ((file->flags & PBL_FS_O_WRITE) == 0) {
    return -EACCES;
  }

  if (file->mp->ops->write == nullptr) {
    return -ENOTSUP;
  }

  return file->mp->ops->write(file, buf, len);
}

off_t pbl_fs_seek(struct pbl_fs_file *file, off_t offset, int whence) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  if ((whence != PBL_FS_SEEK_SET) && (whence != PBL_FS_SEEK_CUR) && (whence != PBL_FS_SEEK_END)) {
    return -EINVAL;
  }

  if (file->mp->ops->lseek == nullptr) {
    return -ENOTSUP;
  }

  return file->mp->ops->lseek(file, offset, whence);
}

off_t pbl_fs_tell(struct pbl_fs_file *file) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  if (file->mp->ops->tell == nullptr) {
    return -ENOTSUP;
  }

  return file->mp->ops->tell(file);
}

off_t pbl_fs_size(struct pbl_fs_file *file) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  if (file->mp->ops->size == nullptr) {
    return -ENOTSUP;
  }

  return file->mp->ops->size(file);
}

int pbl_fs_truncate(struct pbl_fs_file *file, off_t length) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  if ((file->flags & PBL_FS_O_WRITE) == 0) {
    return -EACCES;
  }

  if (length < 0) {
    return -EINVAL;
  }

  if (file->mp->ops->truncate == nullptr) {
    return -ENOTSUP;
  }

  return file->mp->ops->truncate(file, length);
}

int pbl_fs_sync(struct pbl_fs_file *file) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  if (file->mp->ops->sync == nullptr) {
    return 0;
  }

  return file->mp->ops->sync(file);
}

int pbl_fs_close(struct pbl_fs_file *file) {
  if (file->mp == nullptr) {
    return -EBADF;
  }

  // Watches are matched while the backend still holds the name, and run once it is closed
  bool notify = false;
  if (((file->flags & PBL_FS_O_WRITE) != 0) && (file->name != nullptr)) {
    pbl_mutex_lock(&s_watches_lock, PBL_FOREVER);
    for (ListNode *n = s_watches; n != nullptr; n = list_get_next(n)) {
      struct pbl_fs_watch *w = container_of(n, struct pbl_fs_watch, node);
      w->pending =
          ((w->events & PBL_FS_WATCH_CLOSED) != 0) && prv_path_is(w->path, file->mp, file->name);
      notify |= w->pending;
    }
    if (!notify) {
      pbl_mutex_unlock(&s_watches_lock);
    }
  }

  int ret = 0;
  if (file->mp->ops->close != nullptr) {
    ret = file->mp->ops->close(file);
  }

  if (notify) {
    for (ListNode *n = s_watches; n != nullptr; n = list_get_next(n)) {
      struct pbl_fs_watch *w = container_of(n, struct pbl_fs_watch, node);
      if (w->pending && (ret == 0)) {
        w->cb(w->user_data);
      }
      w->pending = false;
    }
    pbl_mutex_unlock(&s_watches_lock);
  }

  *file = (struct pbl_fs_file){};

  return ret;
}

int pbl_fs_unlink(const char *path) {
  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(path, &name);
  if (mp == nullptr) {
    return -ENOENT;
  }

  if (*name == '\0') {
    return -EISDIR;
  }

  if (mp->ops->unlink == nullptr) {
    return -ENOTSUP;
  }

  int ret = mp->ops->unlink(mp, name);
  if (ret == 0) {
    prv_watch_notify(path, PBL_FS_WATCH_REMOVED);
  }

  return ret;
}

int pbl_fs_rename(const char *from, const char *to) {
  const char *from_name;
  const char *to_name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(from, &from_name);
  struct pbl_fs_mount *to_mp = pbl_fs__mount_get(to, &to_name);

  if ((mp == nullptr) || (to_mp == nullptr)) {
    return -ENOENT;
  }

  if (mp != to_mp) {
    return -EXDEV;
  }

  if ((*from_name == '\0') || (*to_name == '\0')) {
    return -EINVAL;
  }

  if (mp->ops->rename == nullptr) {
    return -ENOTSUP;
  }

  int ret = mp->ops->rename(mp, from_name, to_name);
  if (ret == 0) {
    prv_watch_notify(from, PBL_FS_WATCH_REMOVED);
    prv_watch_notify(to, PBL_FS_WATCH_CLOSED);
  }

  return ret;
}

int pbl_fs_mkdir(const char *path) {
  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(path, &name);
  if (mp == nullptr) {
    return -ENOENT;
  }

  if (*name == '\0') {
    return -EEXIST;
  }

  if (mp->ops->mkdir == nullptr) {
    return -ENOTSUP;
  }

  return mp->ops->mkdir(mp, name);
}

int pbl_fs_stat(const char *path, struct pbl_fs_dirent *entry) {
  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(path, &name);
  if (mp == nullptr) {
    return -ENOENT;
  }

  if (*name == '\0') {
    *entry = (struct pbl_fs_dirent){.type = PBL_FS_DIR_ENTRY_DIR};
    return 0;
  }

  if (mp->ops->stat == nullptr) {
    return -ENOTSUP;
  }

  return mp->ops->stat(mp, name, entry);
}

int pbl_fs_statvfs(const char *path, struct pbl_fs_statvfs *stat) {
  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(path, &name);
  if (mp == nullptr) {
    return -ENOENT;
  }

  if (mp->ops->statvfs == nullptr) {
    return -ENOTSUP;
  }

  return mp->ops->statvfs(mp, name, stat);
}

int pbl_fs_opendir(struct pbl_fs_dir *dir, const char *path) {
  *dir = (struct pbl_fs_dir){};

  const char *name;
  struct pbl_fs_mount *mp = pbl_fs__mount_get(path, &name);
  if (mp == nullptr) {
    return -ENOENT;
  }

  if (mp->ops->opendir == nullptr) {
    return -ENOTSUP;
  }

  dir->mp = mp;

  int ret = mp->ops->opendir(dir, name);
  if (ret < 0) {
    *dir = (struct pbl_fs_dir){};
  }

  return ret;
}

int pbl_fs_readdir(struct pbl_fs_dir *dir, struct pbl_fs_dirent *entry) {
  if (dir->mp == nullptr) {
    return -EBADF;
  }

  return dir->mp->ops->readdir(dir, entry);
}

int pbl_fs_closedir(struct pbl_fs_dir *dir) {
  if (dir->mp == nullptr) {
    return -EBADF;
  }

  int ret = 0;
  if (dir->mp->ops->closedir != nullptr) {
    ret = dir->mp->ops->closedir(dir);
  }

  *dir = (struct pbl_fs_dir){};

  return ret;
}

struct pbl_fs_watch *pbl_fs_watch_add(const char *path, uint32_t events, pbl_fs_watch_cb_t cb,
                                      void *user_data) {
  struct pbl_fs_watch *w = kernel_malloc_check(sizeof(*w));
  *w = (struct pbl_fs_watch){
    .path = kernel_strdup_check(path),
    .events = events,
    .cb = cb,
    .user_data = user_data,
  };

  pbl_mutex_lock(&s_watches_lock, PBL_FOREVER);
  s_watches = list_prepend(s_watches, &w->node);
  pbl_mutex_unlock(&s_watches_lock);

  return w;
}

void pbl_fs_watch_remove(struct pbl_fs_watch *watch) {
  pbl_mutex_lock(&s_watches_lock, PBL_FOREVER);
  PBL_ASSERTN(list_contains(s_watches, &watch->node));
  list_remove(&watch->node, &s_watches, nullptr);
  pbl_mutex_unlock(&s_watches_lock);

  kernel_free(watch->path);
  kernel_free(watch);
}
