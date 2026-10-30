/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/fs/fs.h>
#include <pbl/shell/shell.h>
#include <pbl/util/math.h>

#include <kernel/pbl_malloc.h>

static int prv_cmd_ls(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_fs_dir dir;
  int ret = pbl_fs_opendir(&dir, argv[1]);
  if (ret < 0) {
    pbl_shell_error(sh, "opendir failed (%d)", ret);
    return ret;
  }

  struct pbl_fs_dirent *entry = kernel_malloc_check(sizeof(*entry));
  unsigned int count = 0;
  for (;;) {
    ret = pbl_fs_readdir(&dir, entry);
    if ((ret < 0) || (entry->name[0] == '\0')) {
      break;
    }

    pbl_shell_print(sh, "%c %8u %s", (entry->type == PBL_FS_DIR_ENTRY_DIR) ? 'd' : '-',
                    (unsigned int)entry->size, entry->name);
    count++;
  }

  kernel_free(entry);
  pbl_fs_closedir(&dir);

  if (ret < 0) {
    pbl_shell_error(sh, "readdir failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%u entries", count);
  return 0;
}

static int prv_cmd_stat(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_fs_dirent *entry = kernel_malloc_check(sizeof(*entry));
  int ret = pbl_fs_stat(argv[1], entry);
  if (ret == 0) {
    pbl_shell_print(sh, "%s, %u bytes", (entry->type == PBL_FS_DIR_ENTRY_DIR) ? "dir" : "file",
                    (unsigned int)entry->size);
  } else {
    pbl_shell_error(sh, "stat failed (%d)", ret);
  }

  kernel_free(entry);
  return ret;
}

static int prv_cmd_rm(const struct pbl_shell *sh, size_t argc, char **argv) {
  int ret = pbl_fs_unlink(argv[1]);
  if (ret < 0) {
    pbl_shell_error(sh, "unlink failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_df(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_fs_statvfs stat;
  int ret = pbl_fs_statvfs(argv[1], &stat);
  if (ret < 0) {
    pbl_shell_error(sh, "statvfs failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%lu bytes, %lu available", stat.f_blocks * stat.f_frsize,
                  stat.f_bfree * stat.f_frsize);
  return 0;
}

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long offset = 0;
  unsigned long len = 64;

  if ((argc > 2) && (pbl_shell_strtoul(argv[2], &offset) != 0)) {
    pbl_shell_error(sh, "invalid offset '%s'", argv[2]);
    return -EINVAL;
  }

  if ((argc > 3) && (pbl_shell_strtoul(argv[3], &len) != 0)) {
    pbl_shell_error(sh, "invalid length '%s'", argv[3]);
    return -EINVAL;
  }

  struct pbl_fs_file file;
  int ret = pbl_fs_open(&file, argv[1], PBL_FS_O_READ);
  if (ret < 0) {
    pbl_shell_error(sh, "open failed (%d)", ret);
    return ret;
  }

  uint8_t buf[64];
  ret = pbl_fs_seek(&file, offset, PBL_FS_SEEK_SET);
  while ((ret >= 0) && (len > 0)) {
    ret = pbl_fs_read(&file, buf, MIN(len, sizeof(buf)));
    if (ret <= 0) {
      break;
    }

    pbl_shell_hexdump(sh, buf, ret);
    len -= ret;
  }

  pbl_fs_close(&file);

  if (ret < 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  return 0;
}

static const struct pbl_shell_cmd sub_fs[] = {
  PBL_SHELL_CMD_ARG(ls, nullptr, "List a directory <path>", prv_cmd_ls, 2, 0),
  PBL_SHELL_CMD_ARG(stat, nullptr, "Show a file <path>", prv_cmd_stat, 2, 0),
  PBL_SHELL_CMD_ARG(rm, nullptr, "Remove a file <path>", prv_cmd_rm, 2, 0),
  PBL_SHELL_CMD_ARG(df, nullptr, "Show the space of a file system <path>", prv_cmd_df, 2, 0),
  PBL_SHELL_CMD_ARG(read, nullptr, "Dump a file <path> [offset] [length]", prv_cmd_read, 2, 2),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(fs, sub_fs, "File system", nullptr);
