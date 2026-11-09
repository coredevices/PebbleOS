/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pbl/i18n/i18n.h>

#include <clar.h>

static uint8_t s_mo[1024];
static size_t s_mo_size;
static int s_read_error;

static int prv_read(void *data, uint32_t offset, void *buf, size_t length) {
  const uint8_t *mo = data;
  if (s_read_error) {
    return s_read_error;
  }
  if (offset > s_mo_size || length > s_mo_size - offset) {
    return -EINVAL;
  }
  memcpy(buf, &mo[offset], length);
  return 0;
}

static struct pbl_i18n_catalog prv_catalog(void) {
  return (struct pbl_i18n_catalog){
    .read = prv_read,
    .data = s_mo,
    .size = s_mo_size,
  };
}

void test_i18n__initialize(void) {
  FILE *file = fopen(CLAR_FIXTURE_PATH "/i18n/fr_FR.mo", "rb");
  cl_assert(file);
  s_mo_size = fread(s_mo, 1, sizeof(s_mo), file);
  cl_assert(s_mo_size > 0 && s_mo_size < sizeof(s_mo));
  fclose(file);
  s_read_error = 0;

  const struct pbl_i18n_catalog catalog = prv_catalog();
  cl_assert_equal_i(pbl_i18n_set_catalog(&catalog), 0);
}

void test_i18n__cleanup(void) {
  cl_assert_equal_i(pbl_i18n_set_catalog(NULL), 0);
}

static void prv_assert_english(void) {
  cl_assert_equal_s(pbl_i18n_get_locale(), "en_US");
  cl_assert_equal_s(pbl_i18n_get_lang_name(), "English");
  cl_assert_equal_i(pbl_i18n_get_version(), 1);
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Music");
  cl_assert_equal_s(pbl_i18n_ctx_get("Quiet Time", "Enabled", __FILE__), "Enabled");
  pbl_i18n_free_all(__FILE__);
}

void test_i18n__metadata(void) {
  cl_assert_equal_s(pbl_i18n_get_locale(), "fr_FR");
  cl_assert_equal_s(pbl_i18n_get_lang_name(), "Français");
  cl_assert_equal_i(pbl_i18n_get_version(), 7);
}

void test_i18n__get(void) {
  cl_assert_equal_s(pbl_i18n_get("Music", __FILE__), "Musique");
  cl_assert_equal_s(pbl_i18n_get("Untranslated", __FILE__), "Untranslated");
  cl_assert_equal_s(pbl_i18n_get("", __FILE__), "");
  pbl_i18n_free_all(__FILE__);
}

void test_i18n__context(void) {
  cl_assert_equal_s(pbl_i18n_ctx_get("Notifications", "Enabled", __FILE__), "Activées");
  cl_assert_equal_s(pbl_i18n_ctx_get("Quiet Time", "Enabled", __FILE__), "Activé");
  cl_assert_equal_s(pbl_i18n_ctx_get("Alarms", "Enabled", __FILE__), "Enabled");
  cl_assert_equal_s(pbl_i18n_get(pbl_i18n_ctx_noop("Quiet Time", "Enabled"), __FILE__), "Activé");
  pbl_i18n_free_all(__FILE__);
}

void test_i18n__cache_per_owner(void) {
  const char *first = pbl_i18n_get("Music", (void *)0x1);
  cl_assert(pbl_i18n_get("Music", (void *)0x1) == first);
  const char *other = pbl_i18n_get("Music", (void *)0x2);
  cl_assert(other != first);
  pbl_i18n_free_all((void *)0x1);
  cl_assert(pbl_i18n_get("Music", (void *)0x2) == other);
  pbl_i18n_free("Music", (void *)0x2);
  pbl_i18n_free("Music", (void *)0x2);
}

void test_i18n__get_with_buffer(void) {
  char buffer[8];
  pbl_i18n_get_with_buffer("Music", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Musique");
  pbl_i18n_get_with_buffer("Music", buffer, 4);
  cl_assert_equal_s(buffer, "Mus");
  pbl_i18n_get_with_buffer("Untranslated", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Untrans");
  pbl_i18n_ctx_get_with_buffer("Quiet Time", "Enabled", buffer, sizeof(buffer));
  cl_assert_equal_s(buffer, "Activé");
  buffer[0] = 'x';
  pbl_i18n_get_with_buffer("Music", buffer, 0);
  cl_assert_equal_i(buffer[0], 'x');
}

void test_i18n__get_length(void) {
  cl_assert_equal_i(pbl_i18n_get_length("Music"), strlen("Musique"));
  cl_assert_equal_i(pbl_i18n_get_length("Untranslated"), strlen("Untranslated"));
  cl_assert_equal_i(pbl_i18n_ctx_get_length("Alarms", "Enabled"), strlen("Enabled"));
}

void test_i18n__unset(void) {
  const char *translated = pbl_i18n_get("Music", __FILE__);
  cl_assert_equal_s(translated, "Musique");
  cl_assert_equal_i(pbl_i18n_set_catalog(NULL), 0);
  prv_assert_english();
}

void test_i18n__invalid_catalog(void) {
  s_mo[0] ^= 0xff;
  const struct pbl_i18n_catalog catalog = prv_catalog();
  cl_assert_equal_i(pbl_i18n_set_catalog(&catalog), -EINVAL);
  prv_assert_english();
}

void test_i18n__truncated_catalog(void) {
  s_mo_size = 16;
  const struct pbl_i18n_catalog catalog = prv_catalog();
  cl_assert_equal_i(pbl_i18n_set_catalog(&catalog), -EINVAL);
  prv_assert_english();
}

void test_i18n__read_error(void) {
  s_read_error = -EIO;
  const struct pbl_i18n_catalog catalog = prv_catalog();
  cl_assert_equal_i(pbl_i18n_set_catalog(&catalog), -EIO);
  prv_assert_english();
}

void test_i18n__catalog_info(void) {
  cl_assert_equal_i(pbl_i18n_set_catalog(NULL), 0);
  struct pbl_i18n_catalog_info info;
  memset(&info, 0xff, sizeof(info));
  const struct pbl_i18n_catalog catalog = prv_catalog();
  cl_assert_equal_i(pbl_i18n_catalog_get_info(&catalog, &info), 0);
  cl_assert_equal_s(info.locale, "fr_FR");
  cl_assert_equal_s(info.name, "Français");
  cl_assert_equal_i(info.version, 7);
  for (size_t i = strlen(info.name); i < sizeof(info.name); ++i) {
    cl_assert_equal_i(info.name[i], 0);
  }
  cl_assert_equal_s(pbl_i18n_get_locale(), "en_US");

  s_mo[0] ^= 0xff;
  cl_assert_equal_i(pbl_i18n_catalog_get_info(&catalog, &info), -EINVAL);
  s_read_error = -EIO;
  cl_assert_equal_i(pbl_i18n_catalog_get_info(&catalog, &info), -EIO);
}
