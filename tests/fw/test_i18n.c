/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "fixtures/load_test_resources.h"

#include "services/common/i18n/i18n.h"
#include "services/common/i18n/mo.h"
#include "services/normal/filesystem/pfs.h"
#include "resource/resource_ids.auto.h"
#include "flash_region/flash_region.h"

#define I18N_FIXTURE_PATH "i18n"

// Fakes
////////////////////////////////////
#include "fake_spi_flash.h"

// Stubs
////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_language_ui.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_system_reset.h"
#include "stubs_task_watchdog.h"
#include "stubs_memory_layout.h"

// Fakes
/////////////////////////
static bool s_is_english = false;
bool shell_prefs_get_language_english(void) {
  return s_is_english;
}
void shell_prefs_set_language_english(bool english) {
  s_is_english = english;
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

// Setup
/////////////////////////
void test_i18n__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, FRENCH_FIXTURE_NAME, "lang");
  shell_prefs_set_language_english(false);
  i18n_set_resource(RESOURCE_ID_STRINGS);
}

void test_i18n__cleanup(void) {
}

extern I18nString *prv_list_find_string(const char *string, void * owner);

void test_i18n__music(void) {
  const char *first = i18n_get("Music", (void *)0x12345);
  cl_assert(strcmp(first, "Musique") == 0);
  cl_assert(prv_list_find_string("Music", (void *)0x12345) != NULL);
  const char *second = i18n_get("Music", (void *)0x12345);
  cl_assert(first == second);
  const char *third = i18n_get("Music", (void *)0xdeadbeef);
  cl_assert(first != third);
  i18n_free_all((void *)0x12345);
  cl_assert(prv_list_find_string("Music", (void *)0xdeadbeef)->translated_string == third);
  i18n_free_all((void *)0xdeadbeef);
  cl_assert(prv_list_find_string("Music", __FILE__) == NULL);
  // this should be a no-op
  i18n_free("Music", __FILE__);
}

void test_i18n__locale(void) {
  cl_assert(strcmp(i18n_get_locale(), "fr_FR") == 0);
  cl_assert_equal_i(i18n_get_version(), 24);
}

void test_i18n__get_with_buffer(void) {
  const uint32_t LEN = 20;
  char buffer[LEN];
  i18n_get_with_buffer("Music", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Musique");

  i18n_get_with_buffer("abcd abcd abcd abcd", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "abcd abcd abcd abcd");
  cl_assert_equal_i(buffer[LEN - 1], '\0');

  // copied LEN to buffer, i18n should forcibly NULL terminate the buffer
  i18n_get_with_buffer("abcd abcd abcd abcdX", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "abcd abcd abcd abcd");
  cl_assert_equal_i(buffer[LEN - 1], '\0');

  // Make sure we truncate correctly
  i18n_get_with_buffer("Music", buffer, 3);
  cl_assert_equal_s(buffer, "Mu");
}

void test_i18n__get_length(void) {
  const char *first = i18n_get("Music", __FILE__);
  cl_assert_equal_i(strlen(first), i18n_get_length("Music"));
  i18n_free("Music", __FILE__);

  const char *str = "abcd abcd abcd abcd";
  first = i18n_get(str, __FILE__);
  cl_assert_equal_i(strlen(first), i18n_get_length(str));
  i18n_free(str, __FILE__);
}

void test_i18n__ctxt_get(void) {
  const char *ctxt_txt_1 = i18n_ctx_noop("Notifications", "Enabled");
  const char *ctxt_txt_2 = i18n_ctx_noop("Quiet Time", "Enabled");

  const char *first = i18n_ctx_get("Notifications", "Enabled", __FILE__);
  cl_assert(strcmp(first, "Activée") == 0);
  const char *second = i18n_ctx_get("Quiet Time", "Enabled", __FILE__);
  cl_assert(strcmp(second, "Activé") == 0);
  const char *third = i18n_get(ctxt_txt_1, __FILE__);
  cl_assert(third == first);
  const char *fourth = i18n_get(ctxt_txt_2, __FILE__);
  cl_assert(fourth == second);

  i18n_free(ctxt_txt_1, __FILE__);
  cl_assert(prv_list_find_string(ctxt_txt_1, __FILE__) == NULL);
  i18n_ctx_free("Quiet Time", "Enabled", __FILE__);
  cl_assert(prv_list_find_string(ctxt_txt_2, __FILE__) == NULL);
}

void test_i18n__ctxt_get_length(void) {
  const char *first = i18n_ctx_get("badctxt", "Disabled", __FILE__);
  const char *second = i18n_ctx_get("Quiet Time", "Disabled", __FILE__);

  cl_assert_equal_i(strlen(first), i18n_ctx_get_length("badctxt", "Disabled"));
  cl_assert_equal_i(strlen(second), i18n_ctx_get_length("Quiet Time", "Disabled"));

  i18n_free_all(__FILE__);
}

void test_i18n__ctxt_notfound(void) {
  const char *first = i18n_ctx_get("badctxt", "Disabled", __FILE__);
  cl_assert(strcmp(first, "Disabled") == 0);
  const char *second = i18n_ctx_get("Quiet Time", "Disabled", __FILE__);
  cl_assert(strcmp(second, "Désactivé") == 0);

  i18n_ctx_free("badctxt", "Disabled", __FILE__);
  i18n_ctx_free("Quiet Time", "Disabled", __FILE__);
}

void test_i18n__ctxt_get_with_buffer(void) {
  const uint32_t LEN = 20;
  char buffer[LEN];
  i18n_ctx_get_with_buffer("Notifications", "Enabled", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Activée");

  i18n_ctx_get_with_buffer("Quiet Time", "Enabled", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Activé");
}

void test_i18n__reset_language(void) {
  // allocate some crap, to test that freeing works
  const char *first = i18n_get("Music", (void *)0x12345);
  const char *second = i18n_get("abcd", (void *)0x12345);
  shell_prefs_set_language_english(true);
  i18n_set_resource(RESOURCE_ID_STRINGS);
  // reinitialize
  test_i18n__cleanup();
  test_i18n__initialize();
}
