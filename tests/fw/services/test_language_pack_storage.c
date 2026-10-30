/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/services/put_bytes/put_bytes_storage_file.h>

#include <clar.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>
#include <kernel/events.h>
#include <resource/language_pack.h>
#include <resource/resource_ids.auto.h>
#include <shell/prefs.h>
#include <stubs_logging.h>
#include <stubs_memory_layout.h>
#include <stubs_pbl_malloc.h>

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

void event_put(PebbleEvent *event) {
}

static ShellLanguage s_language;

ShellLanguage shell_prefs_get_language(void) {
  return s_language;
}

bool shell_prefs_get_language_english(void) {
  return s_language == ShellLanguageEnglish;
}

void shell_prefs_set_language(ShellLanguage language) {
  s_language = language;
}

void test_language_pack_storage__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");
  s_language = ShellLanguageInstalledPack;
  i18n_set_resource(RESOURCE_ID_STRINGS);
}

static bool prv_add_pack(uint8_t value, bool success) {
  uint8_t buffer[sizeof(PutBytesStorageInfo) + sizeof(LANGUAGE_PACK_ADD_FILENAME)];
  PutBytesStorageInfo *info = (PutBytesStorageInfo *)buffer;
  strcpy(info->filename, LANGUAGE_PACK_ADD_FILENAME);
  PutBytesStorage storage = {};
  const bool initialized = pb_storage_file_init(&storage, ObjectFile, 1, info, 0);
  if (initialized) {
    pb_storage_file_write(&storage, 0, &value, 1);
    storage.current_offset = 1;
  }
  pb_storage_file_deinit(&storage, initialized && success);
  return initialized;
}

static void prv_assert_pack(uint8_t slot, uint8_t value) {
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, language_pack_filename(slot), PBL_FS_O_READ), 0);
  uint8_t data;
  cl_assert_equal_i(pbl_fs_read(&file, &data, 1), 1);
  cl_assert_equal_i(data, value);
  pbl_fs_close(&file);
}

void test_language_pack_storage__fills_slots_without_replacing_existing_packs(void) {
  for (uint8_t slot = 0; slot < LANGUAGE_PACK_COUNT; ++slot) {
    cl_assert(prv_add_pack(slot, true));
  }
  cl_assert(!prv_add_pack(99, true));
  for (uint8_t slot = 0; slot < LANGUAGE_PACK_COUNT; ++slot) {
    prv_assert_pack(slot, slot);
  }
}

void test_language_pack_storage__reuses_removed_slot(void) {
  cl_assert(prv_add_pack(0, true));
  cl_assert(prv_add_pack(1, true));
  cl_assert(prv_add_pack(2, true));
  cl_assert_equal_i(pbl_fs_unlink("/lang1"), 0);
  cl_assert(prv_add_pack(3, true));
  prv_assert_pack(0, 0);
  prv_assert_pack(1, 3);
  prv_assert_pack(2, 2);
}

void test_language_pack_storage__failed_transfer_preserves_existing_pack(void) {
  cl_assert(prv_add_pack(0, true));
  cl_assert(prv_add_pack(1, false));
  prv_assert_pack(0, 0);
  struct pbl_fs_file file;
  cl_assert_equal_i(pbl_fs_open(&file, "/lang1", PBL_FS_O_READ), -ENOENT);
}

void test_language_pack_storage__first_add_keeps_builtin_english_active(void) {
  cl_assert(prv_add_pack(0, true));
  cl_assert_equal_i(s_language, ShellLanguageEnglish);
}

void test_language_pack_storage__extra_add_preserves_selected_pack(void) {
  cl_assert(prv_add_pack(0, true));
  s_language = ShellLanguageInstalledPack;
  cl_assert(prv_add_pack(1, true));
  cl_assert_equal_i(s_language, ShellLanguageInstalledPack);
}

static bool prv_legacy_transfer(uint8_t value, bool success) {
  uint8_t buffer[sizeof(PutBytesStorageInfo) + sizeof("/lang")];
  PutBytesStorageInfo *info = (PutBytesStorageInfo *)buffer;
  strcpy(info->filename, "/lang");
  PutBytesStorage storage = {};
  if (!pb_storage_file_init(&storage, ObjectFile, 1, info, 0)) {
    return false;
  }
  // Old phone apps expect the language to switch on completion, not before.
  cl_assert_equal_i(s_language, ShellLanguagePack2);
  pb_storage_file_write(&storage, 0, &value, 1);
  storage.current_offset = 1;
  pb_storage_file_deinit(&storage, success);
  return true;
}

void test_language_pack_storage__legacy_transfer_selects_slot_zero(void) {
  s_language = ShellLanguagePack2;
  cl_assert(prv_legacy_transfer(5, true));
  cl_assert_equal_i(s_language, ShellLanguageInstalledPack);
  prv_assert_pack(0, 5);
}

void test_language_pack_storage__legacy_transfer_replaces_slot_zero(void) {
  cl_assert(prv_add_pack(0, true));
  cl_assert(prv_add_pack(1, true));
  s_language = ShellLanguagePack2;
  cl_assert(prv_legacy_transfer(7, true));
  prv_assert_pack(0, 7);
  prv_assert_pack(1, 1);
}

void test_language_pack_storage__failed_legacy_transfer_keeps_language(void) {
  s_language = ShellLanguagePack2;
  cl_assert(prv_legacy_transfer(5, false));
  cl_assert_equal_i(s_language, ShellLanguagePack2);
}

void test_language_pack_storage__other_files_are_untouched(void) {
  uint8_t buffer[sizeof(PutBytesStorageInfo) + sizeof("/other")];
  PutBytesStorageInfo *info = (PutBytesStorageInfo *)buffer;
  strcpy(info->filename, "/other");
  PutBytesStorage storage = {};
  cl_assert(pb_storage_file_init(&storage, ObjectFile, 1, info, 0));
  cl_assert_equal_s(info->filename, "/other");
  pb_storage_file_deinit(&storage, true);
  cl_assert_equal_i(s_language, ShellLanguageInstalledPack);
}

void test_language_pack_storage__transfer_blocks_removing_its_slot(void) {
  cl_assert(prv_add_pack(0, true));
  uint8_t buffer[sizeof(PutBytesStorageInfo) + sizeof(LANGUAGE_PACK_ADD_FILENAME)];
  PutBytesStorageInfo *info = (PutBytesStorageInfo *)buffer;
  strcpy(info->filename, LANGUAGE_PACK_ADD_FILENAME);
  PutBytesStorage storage = {};
  cl_assert(pb_storage_file_init(&storage, ObjectFile, 1, info, 0));
  cl_assert_equal_s(info->filename, "/lang1");
  cl_assert(!i18n_remove_pack(1));
  cl_assert(i18n_remove_pack(0));
  pb_storage_file_deinit(&storage, true);
  cl_assert(i18n_remove_pack(1));
}
