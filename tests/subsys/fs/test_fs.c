/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>

#include <clar.h>
#include <fake_pfs.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fs/pfs/pfs.h>

// Recording backend
///////////////////////////////////////////////////////////////////////////////

static struct {
  const struct pbl_fs_mount *mp;
  char name[64];
  uint32_t flags;
  int ret;
} s_rec;

static int prv_rec_open(struct pbl_fs_file *file, const char *name, uint32_t flags) {
  s_rec.mp = file->mp;
  strncpy(s_rec.name, name, sizeof(s_rec.name) - 1);
  s_rec.flags = flags;
  return s_rec.ret;
}

static int prv_rec_close(struct pbl_fs_file *file) {
  return 0;
}

static int prv_rec_unlink(struct pbl_fs_mount *mp, const char *name) {
  s_rec.mp = mp;
  strncpy(s_rec.name, name, sizeof(s_rec.name) - 1);
  return s_rec.ret;
}

static int prv_rec_rename(struct pbl_fs_mount *mp, const char *from, const char *to) {
  s_rec.mp = mp;
  strncpy(s_rec.name, to, sizeof(s_rec.name) - 1);
  return s_rec.ret;
}

static const struct pbl_fs_ops s_rec_ops = {
  .open = prv_rec_open,
  .close = prv_rec_close,
  .unlink = prv_rec_unlink,
  .rename = prv_rec_rename,
};

static struct pbl_fs_mount s_mnt_a = {.mnt_point = "/a", .ops = &s_rec_ops};
static struct pbl_fs_mount s_mnt_ab = {.mnt_point = "/a/b", .ops = &s_rec_ops};

static int s_watch_calls;

static void prv_watch_cb(void *user_data) {
  s_watch_calls += (int)(uintptr_t)user_data;
}

// PFS helpers
///////////////////////////////////////////////////////////////////////////////

static void prv_create(const char *path, size_t size, uint8_t fill) {
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, path, PBL_FS_O_RDWR | PBL_FS_O_CREATE), 0);
  cl_assert_equal_i(pbl_fs_size(&file), 0);
  cl_assert_equal_i(pbl_fs_truncate(&file, size), 0);
  cl_assert_equal_i(pbl_fs_size(&file), size);

  uint8_t buf[size];
  memset(buf, fill, size);
  cl_assert_equal_i(pbl_fs_write(&file, buf, size), size);
  cl_assert_equal_i(pbl_fs_close(&file), 0);
}

static void prv_assert_content(const char *path, size_t size, uint8_t fill) {
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, path, PBL_FS_O_READ), 0);
  cl_assert_equal_i(pbl_fs_size(&file), size);

  uint8_t buf[size];
  cl_assert_equal_i(pbl_fs_read(&file, buf, size), size);
  for (size_t i = 0; i < size; i++) {
    cl_assert_equal_i(buf[i], fill);
  }
  cl_assert_equal_i(pbl_fs_close(&file), 0);
}

static bool prv_exists(const char *path) {
  struct pbl_fs_dirent entry;
  return pbl_fs_stat(path, &entry) == 0;
}

// Setup
///////////////////////////////////////////////////////////////////////////////

void test_fs__initialize(void) {
  s_rec = (typeof(s_rec)){};
  s_watch_calls = 0;
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");
}

void test_fs__cleanup(void) {
  pbl_fs_unmount(&s_mnt_a);
  pbl_fs_unmount(&s_mnt_ab);
  fake_spi_flash_cleanup();
}

// Core
///////////////////////////////////////////////////////////////////////////////

void test_fs__mount_rejects_bad_mount_points(void) {
  struct pbl_fs_mount bad[] = {
    {.mnt_point = "", .ops = &s_rec_ops},  {.mnt_point = "//", .ops = &s_rec_ops},
    {.mnt_point = "a", .ops = &s_rec_ops}, {.mnt_point = "/a/", .ops = &s_rec_ops},
    {.mnt_point = "/a", .ops = nullptr},
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    cl_assert_equal_i(pbl_fs_mount(&bad[i]), -EINVAL);
  }

  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);
  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), -EBUSY);
  struct pbl_fs_mount dup = {.mnt_point = "/a", .ops = &s_rec_ops};
  cl_assert_equal_i(pbl_fs_mount(&dup), -EBUSY);
}

void test_fs__paths_resolve_to_the_longest_mount(void) {
  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);
  cl_assert_equal_i(pbl_fs_mount(&s_mnt_ab), 0);

  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/a/x/y", PBL_FS_O_READ), 0);
  cl_assert(s_rec.mp == &s_mnt_a);
  cl_assert_equal_s(s_rec.name, "x/y");
  pbl_fs_close(&file);

  cl_assert_equal_i(pbl_fs_open(&file, "/a/b/c", PBL_FS_O_READ), 0);
  cl_assert(s_rec.mp == &s_mnt_ab);
  cl_assert_equal_s(s_rec.name, "c");
  pbl_fs_close(&file);

  // A mount point only matches whole path components
  cl_assert_equal_i(pbl_fs_open(&file, "/a/bc", PBL_FS_O_READ), 0);
  cl_assert(s_rec.mp == &s_mnt_a);
  cl_assert_equal_s(s_rec.name, "bc");
  pbl_fs_close(&file);

  cl_assert_equal_i(pbl_fs_open(&file, "/ab", PBL_FS_O_READ), -ENOENT);
  cl_assert_equal_i(pbl_fs_open(&file, "a/x", PBL_FS_O_READ), -ENOENT);
  cl_assert_equal_i(pbl_fs_open(&file, "/a", PBL_FS_O_READ), -EISDIR);

  cl_assert_equal_i(pbl_fs_unmount(&s_mnt_ab), 0);
  cl_assert_equal_i(pbl_fs_open(&file, "/a/b/c", PBL_FS_O_READ), 0);
  cl_assert(s_rec.mp == &s_mnt_a);
  cl_assert_equal_s(s_rec.name, "b/c");
  pbl_fs_close(&file);
}

void test_fs__paths_outside_other_mounts_resolve_to_the_root(void) {
  struct pbl_fs_mount root = {.mnt_point = "/", .ops = &s_rec_ops};
  cl_assert_equal_i(pbl_fs_mount(&root), -EBUSY);

  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);
  prv_create("/ab", 16, 0x11);
  prv_assert_content("/ab", 16, 0x11);

  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/a/x", PBL_FS_O_READ), 0);
  cl_assert(s_rec.mp == &s_mnt_a);
  pbl_fs_close(&file);

  // Unmounted, the path reaches the root file system again
  cl_assert_equal_i(pbl_fs_unmount(&s_mnt_a), 0);
  prv_create("/a/x", 16, 0x22);
  prv_assert_content("/a/x", 16, 0x22);
}

void test_fs__unsupported_operations(void) {
  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);

  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/a/x", PBL_FS_O_RDWR), 0);
  uint8_t byte = 0;
  cl_assert_equal_i(pbl_fs_read(&file, &byte, 1), -ENOTSUP);
  cl_assert_equal_i(pbl_fs_write(&file, &byte, 1), -ENOTSUP);
  cl_assert_equal_i(pbl_fs_seek(&file, 0, PBL_FS_SEEK_SET), -ENOTSUP);
  cl_assert_equal_i(pbl_fs_seek(&file, 0, 42), -EINVAL);
  cl_assert_equal_i(pbl_fs_sync(&file), 0);
  cl_assert_equal_i(pbl_fs_close(&file), 0);
  cl_assert_equal_i(pbl_fs_close(&file), -EBADF);

  cl_assert_equal_i(pbl_fs_mkdir("/a/d"), -ENOTSUP);
  cl_assert_equal_i(pbl_fs_format("/a"), -ENOTSUP);
  cl_assert_equal_i(pbl_fs_format("/a/x"), -ENOENT);
  struct pbl_fs_statvfs stat;
  cl_assert_equal_i(pbl_fs_statvfs("/a", &stat), -ENOTSUP);
}

void test_fs__access_modes(void) {
  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);

  struct pbl_fs_file file;
  uint8_t byte = 0;
  cl_assert_equal_i(pbl_fs_open(&file, "/a/x", PBL_FS_O_READ), 0);
  cl_assert_equal_i(pbl_fs_write(&file, &byte, 1), -EACCES);
  cl_assert_equal_i(pbl_fs_truncate(&file, 1), -EACCES);
  pbl_fs_close(&file);

  cl_assert_equal_i(pbl_fs_open(&file, "/a/x", PBL_FS_O_WRITE), 0);
  cl_assert_equal_i(pbl_fs_read(&file, &byte, 1), -EACCES);
  pbl_fs_close(&file);

  cl_assert_equal_i(pbl_fs_open(&file, "/a/x", PBL_FS_O_REPLACE), 0);
  cl_assert_equal_i(s_rec.flags, PBL_FS_O_REPLACE | PBL_FS_O_WRITE);
  pbl_fs_close(&file);
}

void test_fs__failed_open_leaves_handle_closed(void) {
  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);

  s_rec.ret = -EIO;
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/a/x", PBL_FS_O_RDWR), -EIO);
  cl_assert_equal_i(pbl_fs_close(&file), -EBADF);
}

void test_fs__rename_stays_on_one_mount(void) {
  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);
  cl_assert_equal_i(pbl_fs_rename("/a/x", "/x"), -EXDEV);
  cl_assert_equal_i(pbl_fs_rename("/a/x", "/a/y"), 0);
  cl_assert_equal_s(s_rec.name, "y");
  cl_assert_equal_i(pbl_fs_rename("/x", "/y"), -ENOTSUP);
}

void test_fs__watches(void) {
  struct pbl_fs_watch *closed =
      pbl_fs_watch_add("/w", PBL_FS_WATCH_CLOSED, prv_watch_cb, (void *)1);
  struct pbl_fs_watch *removed =
      pbl_fs_watch_add("/w", PBL_FS_WATCH_REMOVED, prv_watch_cb, (void *)10);
  struct pbl_fs_watch *other =
      pbl_fs_watch_add("/other", PBL_FS_WATCH_ALL, prv_watch_cb, (void *)100);

  prv_create("/w", 16, 0xaa);
  cl_assert_equal_i(s_watch_calls, 1);

  // Read-only use is not a change
  prv_assert_content("/w", 16, 0xaa);
  cl_assert_equal_i(s_watch_calls, 1);

  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/w", PBL_FS_O_RDWR), 0);
  pbl_fs_close(&file);
  cl_assert_equal_i(s_watch_calls, 2);

  cl_assert_equal_i(pbl_fs_unlink("/w"), 0);
  cl_assert_equal_i(s_watch_calls, 12);
  cl_assert_equal_i(pbl_fs_unlink("/w"), -ENOENT);
  cl_assert_equal_i(s_watch_calls, 12);

  pbl_fs_watch_remove(closed);
  pbl_fs_watch_remove(removed);
  prv_create("/w", 16, 0xaa);
  cl_assert_equal_i(s_watch_calls, 12);

  pbl_fs_watch_remove(other);
}

// PFS
///////////////////////////////////////////////////////////////////////////////

void test_fs__pfs_create_needs_a_size(void) {
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/f", PBL_FS_O_READ), -ENOENT);
  cl_assert_equal_i(pbl_fs_open(&file, "/f", PBL_FS_O_RDWR), -ENOENT);

  // A file is only created once it has a size
  cl_assert_equal_i(pbl_fs_open(&file, "/f", PBL_FS_O_RDWR | PBL_FS_O_CREATE), 0);
  cl_assert_equal_i(pbl_fs_size(&file), 0);
  uint8_t byte = 0x55;
  cl_assert_equal_i(pbl_fs_read(&file, &byte, 1), 0);
  cl_assert_equal_i(pbl_fs_write(&file, &byte, 1), -EFBIG);
  cl_assert_equal_i(pbl_fs_truncate(&file, 0), 0);
  cl_assert(!prv_exists("/f"));
  cl_assert_equal_i(pbl_fs_close(&file), 0);
  cl_assert(!prv_exists("/f"));

  prv_create("/f", 100, 0x12);
  prv_assert_content("/f", 100, 0x12);

  struct pbl_fs_dirent entry;
  cl_assert_equal_i(pbl_fs_stat("/f", &entry), 0);
  cl_assert_equal_i(entry.type, PBL_FS_DIR_ENTRY_FILE);
  cl_assert_equal_i(entry.size, 100);
  cl_assert_equal_s(entry.name, "f");
}

void test_fs__pfs_files_have_a_fixed_size(void) {
  prv_create("/f", 10, 0x00);

  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/f", PBL_FS_O_RDWR | PBL_FS_O_CREATE), 0);
  cl_assert_equal_i(pbl_fs_size(&file), 10);
  cl_assert_equal_i(pbl_fs_truncate(&file, 10), 0);
  cl_assert_equal_i(pbl_fs_truncate(&file, 20), -ENOTSUP);
  cl_assert_equal_i(pbl_fs_truncate(&file, 5), -ENOTSUP);

  uint8_t buf[16] = {};
  cl_assert_equal_i(pbl_fs_seek(&file, 8, PBL_FS_SEEK_SET), 8);
  cl_assert_equal_i(pbl_fs_write(&file, buf, 3), -EFBIG);
  cl_assert_equal_i(pbl_fs_tell(&file), 8);
  cl_assert_equal_i(pbl_fs_read(&file, buf, sizeof(buf)), 2);
  cl_assert_equal_i(pbl_fs_read(&file, buf, sizeof(buf)), 0);
  cl_assert_equal_i(pbl_fs_tell(&file), 10);

  cl_assert_equal_i(pbl_fs_seek(&file, -4, PBL_FS_SEEK_END), 6);
  cl_assert_equal_i(pbl_fs_seek(&file, 2, PBL_FS_SEEK_CUR), 8);
  cl_assert_equal_i(pbl_fs_seek(&file, 1, PBL_FS_SEEK_END), -EINVAL);
  cl_assert_equal_i(pbl_fs_seek(&file, -1, PBL_FS_SEEK_SET), -EINVAL);
  cl_assert_equal_i(pbl_fs_tell(&file), 8);
  cl_assert_equal_i(pbl_fs_close(&file), 0);
}

void test_fs__pfs_new_space_reads_as_erased_flash(void) {
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/f", PBL_FS_O_RDWR | PBL_FS_O_CREATE), 0);
  cl_assert_equal_i(pbl_fs_truncate(&file, 4), 0);
  uint8_t buf[4];
  cl_assert_equal_i(pbl_fs_read(&file, buf, sizeof(buf)), sizeof(buf));
  for (size_t i = 0; i < sizeof(buf); i++) {
    cl_assert_equal_i(buf[i], 0xff);
  }
  pbl_fs_close(&file);
}

void test_fs__pfs_replace(void) {
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/r", PBL_FS_O_REPLACE), -ENOENT);

  // With PBL_FS_O_CREATE, a missing file is created as usual
  cl_assert_equal_i(pbl_fs_open(&file, "/r", PBL_FS_O_REPLACE | PBL_FS_O_CREATE), 0);
  cl_assert_equal_i(pbl_fs_truncate(&file, 8), 0);
  uint8_t old_data[8];
  memset(old_data, 0x11, sizeof(old_data));
  cl_assert_equal_i(pbl_fs_write(&file, old_data, sizeof(old_data)), sizeof(old_data));
  cl_assert_equal_i(pbl_fs_close(&file), 0);
  prv_assert_content("/r", 8, 0x11);

  // The current version stays readable, with its size, until the new one is closed
  struct pbl_fs_file current;
  cl_assert_equal_i(pbl_fs_open(&file, "/r", PBL_FS_O_REPLACE | PBL_FS_O_READ), 0);
  cl_assert_equal_i(pbl_fs_size(&file), 0);
  cl_assert_equal_i(pbl_fs_truncate(&file, 12), 0);
  uint8_t new_data[12];
  memset(new_data, 0x22, sizeof(new_data));
  cl_assert_equal_i(pbl_fs_write(&file, new_data, sizeof(new_data)), sizeof(new_data));
  prv_assert_content("/r", 8, 0x11);
  cl_assert_equal_i(pbl_fs_open(&current, "/r", PBL_FS_O_READ), 0);
  cl_assert_equal_i(pbl_fs_size(&current), 8);
  cl_assert_equal_i(pbl_fs_close(&current), 0);
  cl_assert_equal_i(pbl_fs_close(&file), 0);
  prv_assert_content("/r", 12, 0x22);

  // A replacement without a size changes nothing
  cl_assert_equal_i(pbl_fs_open(&file, "/r", PBL_FS_O_REPLACE), 0);
  cl_assert_equal_i(pbl_fs_close(&file), 0);
  prv_assert_content("/r", 12, 0x22);
}

void test_fs__pfs_replace_survives_a_reboot_before_close(void) {
  prv_create("/r", 8, 0x11);

  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/r", PBL_FS_O_REPLACE), 0);
  cl_assert_equal_i(pbl_fs_truncate(&file, 8), 0);
  uint8_t new_data[8];
  memset(new_data, 0x22, sizeof(new_data));
  cl_assert_equal_i(pbl_fs_write(&file, new_data, sizeof(new_data)), sizeof(new_data));

  // Reboot without closing
  extern void pfs_reset_all_state(void);
  pfs_reset_all_state();
  fake_pfs_mount();

  prv_assert_content("/r", 8, 0x11);
}

void test_fs__pfs_open_once(void) {
  prv_create("/f", 4, 0);

  struct pbl_fs_file a;
  struct pbl_fs_file b;
  cl_assert_equal_i(pbl_fs_open(&a, "/f", PBL_FS_O_READ), 0);
  cl_assert_equal_i(pbl_fs_open(&b, "/f", PBL_FS_O_READ), -EBUSY);
  pbl_fs_close(&a);
}

void test_fs__pfs_too_many_open_files(void) {
  struct pbl_fs_file files[9];
  char path[16];
  for (int i = 0; i < 9; i++) {
    snprintf(path, sizeof(path), "/f%d", i);
    prv_create(path, 4, 0);
  }

  for (int i = 0; i < 8; i++) {
    snprintf(path, sizeof(path), "/f%d", i);
    cl_assert_equal_i(pbl_fs_open(&files[i], path, PBL_FS_O_READ), 0);
  }
  cl_assert_equal_i(pbl_fs_open(&files[8], "/f8", PBL_FS_O_READ), -EMFILE);

  for (int i = 0; i < 8; i++) {
    pbl_fs_close(&files[i]);
  }
}

void test_fs__pfs_names(void) {
  char path[sizeof("/") + PBL_FS_NAME_MAX + 1] = "/";
  memset(path + 1, 'n', PBL_FS_NAME_MAX);
  path[1 + PBL_FS_NAME_MAX] = '\0';
  prv_create(path, 4, 0x33);
  prv_assert_content(path, 4, 0x33);

  path[1 + PBL_FS_NAME_MAX] = 'n';
  path[1 + PBL_FS_NAME_MAX + 1] = '\0';
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, path, PBL_FS_O_RDWR | PBL_FS_O_CREATE), -ENAMETOOLONG);

  // No directories: names may contain '/'
  prv_create("/@00000001/app", 4, 0x44);
  prv_assert_content("/@00000001/app", 4, 0x44);
}

void test_fs__pfs_readdir(void) {
  prv_create("/one", 10, 0);
  prv_create("/two", 20, 0);
  prv_create("/@00000001/res", 30, 0);

  struct pbl_fs_dir dir;
  cl_assert_equal_i(pbl_fs_opendir(&dir, "/sub"), -ENOENT);
  cl_assert_equal_i(pbl_fs_opendir(&dir, "/"), 0);

  // Removing files while listing does not change what is listed
  cl_assert_equal_i(pbl_fs_unlink("/two"), 0);

  int seen = 0;
  struct pbl_fs_dirent entry;
  while ((pbl_fs_readdir(&dir, &entry) == 0) && (entry.name[0] != '\0')) {
    cl_assert_equal_i(entry.type, PBL_FS_DIR_ENTRY_FILE);
    if (strcmp(entry.name, "one") == 0) {
      cl_assert_equal_i(entry.size, 10);
      seen |= 1;
    } else if (strcmp(entry.name, "@00000001/res") == 0) {
      cl_assert_equal_i(entry.size, 30);
      seen |= 2;
    } else if (strcmp(entry.name, "two") == 0) {
      seen |= 4;
    }
  }
  cl_assert_equal_i(pbl_fs_closedir(&dir), 0);
  cl_assert(seen & 1);
  cl_assert(seen & 2);

  cl_assert_equal_i(pbl_fs_opendir(&dir, "/"), 0);
  cl_assert_equal_i(pbl_fs_closedir(&dir), 0);
}

void test_fs__pfs_statvfs_and_format(void) {
  struct pbl_fs_statvfs before;
  cl_assert_equal_i(pbl_fs_statvfs("/", &before), 0);
  cl_assert(before.f_blocks > 0);
  cl_assert(before.f_bfree < before.f_blocks);

  prv_create("/big", 10000, 0);
  struct pbl_fs_statvfs after;
  cl_assert_equal_i(pbl_fs_statvfs("/", &after), 0);
  cl_assert(after.f_bfree * after.f_frsize < before.f_bfree * before.f_frsize);

  cl_assert_equal_i(pbl_fs_format("/"), 0);
  cl_assert(!prv_exists("/big"));
  cl_assert_equal_i(pbl_fs_statvfs("/", &after), 0);
  cl_assert_equal_i(after.f_bfree, before.f_bfree);
}

void test_fs__pfs_optimal_size(void) {
  cl_assert_equal_i(pbl_fs_pfs_optimal_size("/abc", 10),
                    pfs_sector_optimal_size(10, strlen("abc")));
  cl_assert_equal_i(pbl_fs_pfs_optimal_size("abc", 10), -EINVAL);

  cl_assert_equal_i(pbl_fs_mount(&s_mnt_a), 0);
  cl_assert_equal_i(pbl_fs_pfs_optimal_size("/a/abc", 10), -EINVAL);
}

void test_fs__pfs_format_hint_flags(void) {
  prv_create("/f", 4, 0x77);

  struct pbl_fs_file file;
  cl_assert_equal_i(
      pbl_fs_open(&file, "/f",
                  PBL_FS_O_READ | PBL_FS_O_PFS_SKIP_HDR_CRC_CHECK | PBL_FS_O_PFS_USE_PAGE_CACHE),
      0);
  uint8_t buf[4];
  cl_assert_equal_i(pbl_fs_read(&file, buf, sizeof(buf)), sizeof(buf));
  cl_assert_equal_i(buf[0], 0x77);
  pbl_fs_close(&file);
}

// Files written through the PFS API and through the file system API are the same files.
void test_fs__pfs_on_flash_compatibility(void) {
  int fd = pfs_open("legacy", OP_FLAG_WRITE, FILE_TYPE_STATIC, 6);
  cl_assert(fd >= 0);
  cl_assert_equal_i(pfs_write(fd, "legacy", 6), 6);
  pfs_close(fd);

  struct pbl_fs_file file;
  char buf[6];
  cl_assert_equal_i(pbl_fs_open(&file, "/legacy", PBL_FS_O_READ), 0);
  cl_assert_equal_i(pbl_fs_read(&file, buf, sizeof(buf)), sizeof(buf));
  cl_assert(memcmp(buf, "legacy", 6) == 0);
  pbl_fs_close(&file);

  cl_assert_equal_i(fake_pfs_create(&file, "/modern", 6), 0);
  cl_assert_equal_i(pbl_fs_write(&file, "modern", 6), 6);
  pbl_fs_close(&file);

  // Survives a reboot
  extern void pfs_reset_all_state(void);
  pfs_reset_all_state();
  fake_pfs_mount();

  fd = pfs_open("modern", OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  cl_assert(fd >= 0);
  cl_assert_equal_i(pfs_get_file_size(fd), 6);
  cl_assert_equal_i(pfs_read(fd, buf, sizeof(buf)), sizeof(buf));
  cl_assert(memcmp(buf, "modern", 6) == 0);
  pfs_close(fd);
}
