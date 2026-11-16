/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>

#define GREETING "/sample-greeting"
#define BOOTS    "/sample-boots"
#define PREFIX   "sample-"

/* PFS, the file system on the external flash, mounted at the root. */
static struct pbl_fs_mount s_mount = {.mnt_point = "/", .ops = &pbl_fs_pfs_ops};

static unsigned int s_closed;

static void prv_closed(void *user_data) {
  s_closed++;
}

/* PFS files have a fixed size, given with pbl_fs_truncate() before the first write. */
static int prv_write_file(const char *path, const void *data, size_t len) {
  struct pbl_fs_file file;
  ssize_t written;
  int ret;

  /* The new version replaces the old one only once closed: a reset leaves either. */
  ret = pbl_fs_open(&file, path, PBL_FS_O_WRITE | PBL_FS_O_CREATE | PBL_FS_O_REPLACE);
  if (ret < 0) {
    return ret;
  }

  ret = pbl_fs_truncate(&file, (off_t)len);
  if (ret == 0) {
    written = pbl_fs_write(&file, data, len);
    if (written < 0) {
      ret = (int)written;
    }
  }

  (void)pbl_fs_close(&file);

  return ret;
}

static int prv_count_boots(void) {
  struct pbl_fs_file file;
  uint32_t boots = 0;
  int ret;

  ret = pbl_fs_open(&file, BOOTS, PBL_FS_O_READ);
  if (ret == 0) {
    if (pbl_fs_read(&file, &boots, sizeof(boots)) != sizeof(boots)) {
      boots = 0;
    }
    (void)pbl_fs_close(&file);
  } else if (ret != -ENOENT) {
    return ret;
  }

  boots++;
  printf("fs: run %u of this sample on this flash\n", (unsigned int)boots);

  return prv_write_file(BOOTS, &boots, sizeof(boots));
}

int main(void) {
  static const char greeting[] = "Hello, flash!";
  struct pbl_fs_statvfs vfs;
  struct pbl_fs_dirent entry;
  struct pbl_fs_file file;
  struct pbl_fs_dir dir;
  char buf[32] = {0};
  ssize_t len;
  int ret;

  ret = pbl_fs_mount(&s_mount);
  if (ret < 0) {
    fprintf(stderr, "fs: mount failed (%d)\n", ret);
    return ret;
  }

  ret = pbl_fs_statvfs("/", &vfs);
  if (ret < 0) {
    fprintf(stderr, "fs: statvfs failed (%d)\n", ret);
    return ret;
  }
  printf("fs: %lu KiB, %lu KiB free\n", vfs.f_blocks * vfs.f_frsize / 1024U,
         vfs.f_bfree * vfs.f_frsize / 1024U);

  ret = prv_count_boots();
  if (ret < 0) {
    fprintf(stderr, "fs: boot count not saved (%d)\n", ret);
    return ret;
  }

  struct pbl_fs_watch *watch = pbl_fs_watch_add(GREETING, PBL_FS_WATCH_CLOSED, prv_closed, nullptr);

  ret = prv_write_file(GREETING, greeting, strlen(greeting));
  if (ret < 0) {
    fprintf(stderr, "fs: write failed (%d)\n", ret);
    return ret;
  }
  printf("fs: watch saw the file closed after writing: %s\n", s_closed == 1 ? "yes" : "no");
  pbl_fs_watch_remove(watch);

  ret = pbl_fs_open(&file, GREETING, PBL_FS_O_READ);
  if (ret < 0) {
    fprintf(stderr, "fs: open failed (%d)\n", ret);
    return ret;
  }
  len = pbl_fs_read(&file, buf, sizeof(buf) - 1);
  printf("fs: read %d of %d bytes: %s\n", (int)len, (int)pbl_fs_size(&file), buf);

  memset(buf, 0, sizeof(buf));
  (void)pbl_fs_seek(&file, 7, PBL_FS_SEEK_SET);
  len = pbl_fs_read(&file, buf, sizeof(buf) - 1);
  printf("fs: from offset 7: %s\n", len > 0 ? buf : "nothing");
  (void)pbl_fs_close(&file);

  /* Writing past the end of a fixed-size file fails. */
  ret = pbl_fs_open(&file, GREETING, PBL_FS_O_WRITE);
  if (ret < 0) {
    fprintf(stderr, "fs: open failed (%d)\n", ret);
    return ret;
  }
  (void)pbl_fs_seek(&file, 0, PBL_FS_SEEK_END);
  printf("fs: one more byte: %s\n", pbl_fs_write(&file, "!", 1) == -EFBIG ? "-EFBIG" : "fits");
  (void)pbl_fs_close(&file);

  /* Entries come in no particular order, and the flash may hold other files. */
  ret = pbl_fs_opendir(&dir, "/");
  if (ret < 0) {
    fprintf(stderr, "fs: opendir failed (%d)\n", ret);
    return ret;
  }
  while (pbl_fs_readdir(&dir, &entry) == 0 && entry.name[0] != '\0') {
    if (strncmp(entry.name, PREFIX, strlen(PREFIX)) == 0) {
      printf("fs: listed %s, %u bytes\n", entry.name, (unsigned int)entry.size);
    }
  }
  (void)pbl_fs_closedir(&dir);

  ret = pbl_fs_unlink(GREETING);
  if (ret < 0) {
    fprintf(stderr, "fs: unlink failed (%d)\n", ret);
    return ret;
  }
  printf("fs: after removing it, stat returns %s\n",
         pbl_fs_stat(GREETING, &entry) == -ENOENT ? "-ENOENT" : "an entry");

  printf("Sample done\n");

  return 0;
}
