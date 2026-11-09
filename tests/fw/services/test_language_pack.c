/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/i18n/i18n.h>
#include <pbl/services/language_pack.h>

#include <clar.h>
#include <fixtures/load_test_resources.h>
#include <kernel/events.h>
#include <resource/language_pack.h>
#include <resource/resource_ids.auto.h>
#include <shell/prefs.h>

// Fakes
////////////////////////////////////
#include <fake_pfs.h>
#include <fake_spi_flash.h>

// Stubs
////////////////////////////////////

// Fakes
/////////////////////////
static bool s_is_english = false;
bool shell_prefs_get_language_english(void) {
  return s_is_english;
}
void shell_prefs_set_language_english(bool english) {
  s_is_english = english;
}
static ShellLanguage s_language;
ShellLanguage shell_prefs_get_language(void) {
  return s_language;
}
void shell_prefs_set_language(ShellLanguage language) {
  s_language = language;
  shell_prefs_set_language_english(language == ShellLanguageEnglish);
  const uint8_t slot = shell_language_pack_slot(language);
  language_pack_set_resource(
      language_pack_resource_id(slot, RESOURCE_ID_STRINGS, RESOURCE_ID_STRINGS));
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

static int s_num_language_change_events;
void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_LANGUAGE_CHANGE_EVENT) {
    s_num_language_change_events++;
  }
}

// Setup
/////////////////////////
void test_language_pack__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, FRENCH_FIXTURE_NAME, "/lang");
  s_language = ShellLanguageInstalledPack;
  shell_prefs_set_language_english(false);
  language_pack_set_resource(RESOURCE_ID_STRINGS);
}

void test_language_pack__cleanup(void) {
}

void test_language_pack__music(void) {
  const char *first = pbl_i18n_get("Music", (void *)0x12345);
  cl_assert(strcmp(first, "Musique") == 0);
  const char *second = pbl_i18n_get("Music", (void *)0x12345);
  cl_assert(first == second);
  const char *third = pbl_i18n_get("Music", (void *)0xdeadbeef);
  cl_assert(first != third);
  pbl_i18n_free_all((void *)0x12345);
  cl_assert(pbl_i18n_get("Music", (void *)0xdeadbeef) == third);
  pbl_i18n_free_all((void *)0xdeadbeef);
  // this should be a no-op
  pbl_i18n_free("Music", __FILE__);
}

void test_language_pack__locale(void) {
  cl_assert(strcmp(pbl_i18n_get_locale(), "fr_FR") == 0);
  cl_assert_equal_i(pbl_i18n_get_version(), 24);
}

void test_language_pack__english_with_installed_pack(void) {
  shell_prefs_set_language_english(true);
  language_pack_set_resource(RESOURCE_ID_STRINGS);

  cl_assert_equal_s(pbl_i18n_get_locale(), "en_US");
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Music");
  pbl_i18n_free_all(__FILE__);
}

void test_language_pack__english_without_installed_pack(void) {
  cl_assert_equal_i(pbl_fs_unlink("/lang"), 0);
  language_pack_set_resource(RESOURCE_ID_STRINGS);

  cl_assert_equal_s(pbl_i18n_get_locale(), "en_US");
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Music");
  pbl_i18n_free_all(__FILE__);
}

void test_language_pack__get_with_buffer(void) {
  const uint32_t LEN = 20;
  char buffer[LEN];
  pbl_i18n_get_with_buffer("Music", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Musique");

  pbl_i18n_get_with_buffer("abcd abcd abcd abcd", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "abcd abcd abcd abcd");
  cl_assert_equal_i(buffer[LEN - 1], '\0');

  // copied LEN to buffer, i18n should forcibly NULL terminate the buffer
  pbl_i18n_get_with_buffer("abcd abcd abcd abcdX", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "abcd abcd abcd abcd");
  cl_assert_equal_i(buffer[LEN - 1], '\0');

  // Make sure we truncate correctly
  pbl_i18n_get_with_buffer("Music", buffer, 3);
  cl_assert_equal_s(buffer, "Mu");
}

void test_language_pack__get_length(void) {
  const char *first = pbl_i18n_get("Music", __FILE__);
  cl_assert_equal_i(strlen(first), pbl_i18n_get_length("Music"));
  pbl_i18n_free("Music", __FILE__);

  const char *str = "abcd abcd abcd abcd";
  first = pbl_i18n_get(str, __FILE__);
  cl_assert_equal_i(strlen(first), pbl_i18n_get_length(str));
  pbl_i18n_free(str, __FILE__);
}

void test_language_pack__ctxt_get(void) {
  const char *ctxt_txt_1 = pbl_i18n_ctx_noop("Notifications", "Enabled");
  const char *ctxt_txt_2 = pbl_i18n_ctx_noop("Quiet Time", "Enabled");

  const char *first = pbl_i18n_ctx_get("Notifications", "Enabled", __FILE__);
  cl_assert(strcmp(first, "Activée") == 0);
  const char *second = pbl_i18n_ctx_get("Quiet Time", "Enabled", __FILE__);
  cl_assert(strcmp(second, "Activé") == 0);
  const char *third = pbl_i18n_get(ctxt_txt_1, __FILE__);
  cl_assert(third == first);
  const char *fourth = pbl_i18n_get(ctxt_txt_2, __FILE__);
  cl_assert(fourth == second);

  pbl_i18n_free(ctxt_txt_1, __FILE__);
  pbl_i18n_ctx_free("Quiet Time", "Enabled", __FILE__);
}

void test_language_pack__ctxt_get_length(void) {
  const char *first = pbl_i18n_ctx_get("badctxt", "Disabled", __FILE__);
  const char *second = pbl_i18n_ctx_get("Quiet Time", "Disabled", __FILE__);

  cl_assert_equal_i(strlen(first), pbl_i18n_ctx_get_length("badctxt", "Disabled"));
  cl_assert_equal_i(strlen(second), pbl_i18n_ctx_get_length("Quiet Time", "Disabled"));

  pbl_i18n_free_all(__FILE__);
}

void test_language_pack__ctxt_notfound(void) {
  const char *first = pbl_i18n_ctx_get("badctxt", "Disabled", __FILE__);
  cl_assert(strcmp(first, "Disabled") == 0);
  const char *second = pbl_i18n_ctx_get("Quiet Time", "Disabled", __FILE__);
  cl_assert(strcmp(second, "Désactivé") == 0);

  pbl_i18n_ctx_free("badctxt", "Disabled", __FILE__);
  pbl_i18n_ctx_free("Quiet Time", "Disabled", __FILE__);
}

void test_language_pack__ctxt_get_with_buffer(void) {
  const uint32_t LEN = 20;
  char buffer[LEN];
  pbl_i18n_ctx_get_with_buffer("Notifications", "Enabled", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Activée");

  pbl_i18n_ctx_get_with_buffer("Quiet Time", "Enabled", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Activé");
}

void test_language_pack__reset_language(void) {
  // allocate some crap, to test that freeing works
  pbl_i18n_get("Music", (void *)0x12345);
  pbl_i18n_get("abcd", (void *)0x12345);
  shell_prefs_set_language_english(true);
  language_pack_set_resource(RESOURCE_ID_STRINGS);
  // reinitialize
  test_language_pack__cleanup();
  test_language_pack__initialize();
}

void test_language_pack__language_change_event(void) {
  s_num_language_change_events = 0;

  language_pack_set_resource(RESOURCE_ID_STRINGS);
  cl_assert_equal_i(s_num_language_change_events, 0);

  language_pack_enable(false);
  cl_assert_equal_i(s_num_language_change_events, 1);
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Music");
  pbl_i18n_free_all(__FILE__);

  language_pack_enable(false);
  cl_assert_equal_i(s_num_language_change_events, 1);

  language_pack_enable(true);
  cl_assert_equal_i(s_num_language_change_events, 2);
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Musique");
  pbl_i18n_free_all(__FILE__);

  shell_prefs_set_language_english(true);
  language_pack_set_resource(RESOURCE_ID_STRINGS);
  cl_assert_equal_i(s_num_language_change_events, 3);
}

void test_language_pack__extra_pack_metadata_preserves_translations(void) {
  const char *translated = pbl_i18n_get("Music", __FILE__);
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  struct pbl_i18n_catalog_info pack;
  cl_assert(language_pack_get_info(1, &pack));
  cl_assert_equal_s(pack.locale, "zh_CN");
  for (size_t i = strlen(pack.name); i < sizeof(pack.name); ++i) {
    cl_assert_equal_i(pack.name[i], 0);
  }
  cl_assert_equal_s(pbl_i18n_get_locale(), "fr_FR");
  cl_assert_equal_s(translated, "Musique");
  cl_assert(pbl_i18n_get("Music", __FILE__) == translated);
  pbl_i18n_free_all(__FILE__);
}

void test_language_pack__select_extra_pack_and_remove(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, FRENCH_FIXTURE_NAME, "/lang2");
  shell_prefs_set_language(ShellLanguagePack2);
  cl_assert_equal_s(pbl_i18n_get_locale(), "fr_FR");
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Musique");
  cl_assert(language_pack_remove(2));
  cl_assert_equal_s(pbl_i18n_get_locale(), "en_US");
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Music");
  struct pbl_i18n_catalog_info pack;
  cl_assert(language_pack_get_info(0, &pack));
  cl_assert(!language_pack_get_info(2, &pack));
}

void test_language_pack__remove_resets_selection_with_english_toggled(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, FRENCH_FIXTURE_NAME, "/lang2");
  shell_prefs_set_language(ShellLanguagePack2);
  shell_prefs_set_language_english(true);
  cl_assert(language_pack_remove(2));
  cl_assert_equal_i(shell_prefs_get_language(), ShellLanguageEnglish);
}

void test_language_pack__installation_preserves_english(void) {
  shell_prefs_set_language(ShellLanguageEnglish);
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, FRENCH_FIXTURE_NAME, "/lang");
  cl_assert(shell_prefs_get_language_english());
  cl_assert_equal_s(pbl_i18n_get_locale(), "en_US");
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Music");
}

void test_language_pack__missing_and_invalid_slots(void) {
  struct pbl_i18n_catalog_info pack;
  cl_assert(!language_pack_get_info(1, &pack));
  cl_assert(!language_pack_get_info(LANGUAGE_PACK_COUNT, &pack));
  cl_assert(!language_pack_get_info(0, NULL));
  cl_assert(!language_pack_remove(LANGUAGE_PACK_COUNT));
  cl_assert_equal_s(pbl_i18n_get_locale(), "fr_FR");
}

void test_language_pack__invalid_pack_remains_removable(void) {
  struct pbl_fs_file file;
  cl_assert_equal_i(fake_pfs_create(&file, "/lang3", 1), 0);
  const uint8_t invalid = 0;
  cl_assert_equal_i(pbl_fs_write(&file, &invalid, 1), 1);
  pbl_fs_close(&file);
  struct pbl_i18n_catalog_info pack;
  cl_assert(!language_pack_get_info(3, &pack));
  cl_assert(language_pack_is_installed(3));
  cl_assert(language_pack_remove(3));
  cl_assert(!language_pack_is_installed(3));
  cl_assert_equal_s(pbl_i18n_get_locale(), "fr_FR");
}
