/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/settings/settings.h>

#define FILE_NAME  "/sample-settings"
#define FILE_SPACE 1024

/* Keys and values are bytes; these keys are strings without their terminator. */
#define KEY(str) (str), (sizeof(str) - 1U)

static struct pbl_fs_mount s_mount = {.mnt_point = "/", .ops = &pbl_fs_pfs_ops};
static struct pbl_settings_file s_file;

static void prv_changed(const void *key, size_t key_len, uint32_t last_modified, void *user_data) {
  printf("settings: changed %.*s\n", (int)key_len, (const char *)key);
}

static struct pbl_settings_watch s_watch = {
  .name = FILE_NAME,
  .cb = prv_changed,
};

static bool prv_print_record(struct pbl_settings_file *file, struct pbl_settings_record *info,
                             void *context) {
  char key[PBL_SETTINGS_KEY_MAX_LEN + 1] = {0};
  unsigned int *count = context;

  /* Deleted records are visited too, with no value. */
  if (info->val_len == 0) {
    return true;
  }

  pbl_settings_read_key(file, key, info->key_len);
  printf("settings: record %s, %u bytes\n", key, (unsigned int)info->val_len);
  (*count)++;

  return true;
}

int main(void) {
  char name[16] = {0};
  uint32_t runs = 0;
  uint8_t level = 3;
  unsigned int count = 0;
  size_t len;
  int ret;

  ret = pbl_fs_mount(&s_mount);
  if (ret < 0) {
    fprintf(stderr, "settings: mount failed (%d)\n", ret);
    return ret;
  }

  /* Watches are registered once, before the file is written, and see every later write. */
  pbl_settings_watch_add(&s_watch);

  ret = pbl_settings_open(&s_file, FILE_NAME, FILE_SPACE);
  if (ret < 0) {
    fprintf(stderr, "settings: open failed (%d)\n", ret);
    return ret;
  }

  /* A value kept from the last run, if any. */
  if (pbl_settings_get(&s_file, KEY("runs"), &runs, sizeof(runs)) == -ENOENT) {
    printf("settings: first run on this flash\n");
  }
  runs++;
  ret = pbl_settings_set(&s_file, KEY("runs"), &runs, sizeof(runs));
  if (ret < 0) {
    fprintf(stderr, "settings: set failed (%d)\n", ret);
    goto close;
  }
  printf("settings: run %u\n", (unsigned int)runs);

  ret = pbl_settings_set(&s_file, KEY("name"), "Pebble", strlen("Pebble"));
  if (ret == 0) {
    ret = pbl_settings_set(&s_file, KEY("level"), &level, sizeof(level));
  }
  if (ret < 0) {
    fprintf(stderr, "settings: set failed (%d)\n", ret);
    goto close;
  }

  /* Leave room for the terminator. */
  len = pbl_settings_get_len(&s_file, KEY("name"));
  ret = len < sizeof(name) ? pbl_settings_get(&s_file, KEY("name"), name, len) : -ENOSPC;
  printf("settings: name is %s\n", ret == 0 ? name : "missing");

  level = 0;
  ret = pbl_settings_get(&s_file, KEY("level"), &level, sizeof(level));
  printf("settings: level is %u\n", ret == 0 ? level : 0U);

  printf(
      "settings: reading 4 bytes of a 1-byte value: %s\n",
      pbl_settings_get(&s_file, KEY("level"), &runs, sizeof(runs)) == -ERANGE ? "-ERANGE" : "read");

  pbl_settings_each(&s_file, prv_print_record, &count);
  printf("settings: %u records\n", count);

  ret = pbl_settings_delete(&s_file, KEY("level"));
  if (ret < 0) {
    fprintf(stderr, "settings: delete failed (%d)\n", ret);
    goto close;
  }
  printf("settings: level exists after deleting it: %s\n",
         pbl_settings_exists(&s_file, KEY("level")) ? "yes" : "no");

  printf("Sample done\n");

close:
  pbl_settings_close(&s_file);

  return ret;
}
