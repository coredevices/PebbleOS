/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/text_resources.h>
#include <clar.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>
#include <fixtures/load_test_resources.h>
#include <resource/language_pack.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>
#include <resource/system_resource.h>
#include <shell/prefs.h>
#include <stubs_logging.h>
#include <stubs_memory_layout.h>
#include <stubs_pbl_malloc.h>
#include <stubs_syscall_internal.h>
#include <syscall/syscall.h>

static ShellLanguage s_language;

ShellLanguage shell_prefs_get_language(void) {
  return s_language;
}

bool shell_prefs_get_language_english(void) {
  return s_language == ShellLanguageEnglish;
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

void fw_reset_into_prf(void) {
  cl_fail("fw_reset_into_prf");
}

void launcher_panic(uint32_t error_code) {
  cl_fail("launcher_panic");
}

extern FontInfo s_system_fonts_info_table[];

static uint32_t prv_extension(uint8_t slot) {
  return language_pack_resource_id(slot, RESOURCE_ID_GOTHIC_18_EXTENDED, RESOURCE_ID_STRINGS);
}

void test_system_resource_language_fonts__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();
  s_language = ShellLanguageEnglish;
  system_resource_init();
  system_resource_language_changed();
}

void test_system_resource_language_fonts__cleanup(void) {
  // Pack fonts outlive pbl_fs_format(); removing the files drops them.
  for (uint8_t slot = 0; slot < LANGUAGE_PACK_COUNT; ++slot) {
    pbl_fs_unlink(language_pack_filename(slot));
  }
}

void test_system_resource_language_fonts__no_packs(void) {
  FontInfo font;
  for (uint8_t slot = 0; slot <= LANGUAGE_PACK_COUNT; ++slot) {
    cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, slot, &font));
  }
}

void test_system_resource_language_fonts__finds_fonts_per_slot(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang3");
  FontInfo font;
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 0, &font));
  cl_assert(sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 1, &font));
  cl_assert_equal_i(font.base.resource_id, prv_extension(1));
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 2, &font));
  cl_assert(sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 3, &font));
  cl_assert_equal_i(font.base.resource_id, prv_extension(3));
  // A second lookup is served from the loaded table
  cl_assert(sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 3, &font));
  cl_assert_equal_i(font.base.resource_id, prv_extension(3));
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, LANGUAGE_PACK_COUNT, &font));
}

void test_system_resource_language_fonts__skips_primary_pack(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang2");
  s_language = ShellLanguagePack2;
  system_resource_language_changed();
  FontInfo font;
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 2, &font));
  s_language = ShellLanguageEnglish;
  system_resource_language_changed();
  cl_assert(sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 2, &font));
}

void test_system_resource_language_fonts__font_without_extension(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  FontInfo font;
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_24, 1, &font));
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_14, 1, &font));
}

void test_system_resource_language_fonts__pack_changes_invalidate(void) {
  FontInfo font;
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 1, &font));
  GFont system_font = system_resource_get_font(FONT_KEY_GOTHIC_18);
  cl_assert(system_font && system_font->loaded);
  const uint32_t generation = g_font_language_generation;

  // The absent pack was cached; installing it must be noticed.
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  cl_assert(g_font_language_generation != generation);
  cl_assert(!system_font->loaded);
  cl_assert(sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 1, &font));

  cl_assert_equal_i(pbl_fs_unlink("/lang1"), 0);
  cl_assert(!sys_font_get_language_font(RESOURCE_ID_GOTHIC_18, 1, &font));
}

void test_system_resource_language_fonts__pack_changes_drop_cached_resources(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  s_language = ShellLanguagePack1;
  system_resource_language_changed();
  cl_assert(system_resource_get_font(FONT_KEY_GOTHIC_18));
  cl_assert(resource_size(SYSTEM_APP, prv_extension(1)) > 0);
  cl_assert_equal_i(pbl_fs_unlink("/lang1"), 0);
  cl_assert_equal_i(resource_size(SYSTEM_APP, prv_extension(1)), 0);
}

void test_system_resource_language_fonts__generation_is_a_plain_read(void) {
  const uint32_t generation = g_font_language_generation;
  s_language = ShellLanguagePack1;
  cl_assert_equal_i(g_font_language_generation, generation);
  system_resource_language_changed();
  cl_assert(g_font_language_generation != generation);
}
