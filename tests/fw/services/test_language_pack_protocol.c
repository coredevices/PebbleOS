/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/services/i18n/i18n.h>

#include <clar.h>
#include <kernel/language_pack_protocol.h>
#include <kernel/system_message.h>
#include <resource/language_pack.h>
#include <shell/prefs.h>

static bool s_present[4];
static bool s_valid[4];
static bool s_fonts[4];
static uint8_t s_response[164];
static size_t s_length;
static ShellLanguage s_language;
static int s_removed;

bool i18n_get_pack_info(uint8_t slot, I18nPackInfo *info) {
  if (slot >= 4 || !s_valid[slot]) {
    return false;
  }
  *info = (I18nPackInfo){.locale = "fr_FR", .name = "Français", .version = 0x1234};
  return true;
}

bool i18n_pack_has_fonts(uint8_t slot) {
  return slot < 4 && s_fonts[slot];
}

bool language_pack_is_installed(uint8_t slot) {
  return slot < 4 && s_present[slot];
}

bool i18n_remove_pack(uint8_t slot) {
  if (!language_pack_is_installed(slot)) {
    return false;
  }
  s_present[slot] = false;
  s_valid[slot] = false;
  s_fonts[slot] = false;
  s_removed = slot;
  return true;
}

ShellLanguage shell_prefs_get_language(void) {
  return s_language;
}

bool shell_prefs_get_language_english(void) {
  return s_language == ShellLanguageEnglish;
}

void shell_prefs_set_language(ShellLanguage language) {
  s_language = language;
}

bool comm_session_send_data(CommSession *session, uint16_t endpoint, const uint8_t *data,
                            size_t length, uint32_t timeout) {
  cl_assert_equal_i(endpoint, SYS_MSG_ENDPOINT_ID);
  cl_assert(length <= sizeof(s_response));
  memcpy(s_response, data, length);
  s_length = length;
  return true;
}

void test_language_pack_protocol__initialize(void) {
  memset(s_present, 0, sizeof(s_present));
  memset(s_valid, 0, sizeof(s_valid));
  memset(s_fonts, 0, sizeof(s_fonts));
  s_present[0] = s_valid[0] = true;
  s_present[2] = true;
  s_language = ShellLanguageEnglish;
  s_removed = -1;
  s_length = 0;
}

void test_language_pack_protocol__lists_valid_invalid_and_empty_slots(void) {
  const uint8_t request[] = {0, SysMsgLanguagePacks, 7};
  language_pack_protocol_handle_list(NULL, request, sizeof(request));
  cl_assert_equal_i(s_length, 164);
  cl_assert_equal_i(s_response[1], SysMsgLanguagePacksResponse);
  cl_assert_equal_i(s_response[2], 7);
  cl_assert_equal_i(s_response[3], 0xff);
  cl_assert_equal_i(s_response[5], 7);
  cl_assert_equal_i(s_response[6], 0x34);
  cl_assert_equal_i(s_response[7], 0x12);
  cl_assert_equal_s((char *)s_response + 8, "fr_FR");
  cl_assert_equal_s((char *)s_response + 14, "Français");
  cl_assert_equal_i(s_response[45], 0);
  cl_assert_equal_i(s_response[84], 2);
  cl_assert_equal_i(s_response[85], 1);
}

void test_language_pack_protocol__lists_font_only_pack_as_readable(void) {
  s_present[1] = s_fonts[1] = true;
  s_language = ShellLanguagePack1;
  const uint8_t request[] = {0, SysMsgLanguagePacks, 7};
  language_pack_protocol_handle_list(NULL, request, sizeof(request));
  cl_assert_equal_i(s_response[3], 0xff);
  cl_assert_equal_i(s_response[45], 3);
  cl_assert_equal_i(s_response[46], 0);
  cl_assert_equal_i(s_response[47], 0);
  cl_assert_equal_s((char *)s_response + 48, "");
}

void test_language_pack_protocol__removes_font_only_pack(void) {
  s_present[1] = s_fonts[1] = true;
  const uint8_t request[] = {0, SysMsgLanguagePackRemove, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  language_pack_protocol_handle_remove(NULL, request, sizeof(request));
  cl_assert_equal_i(s_response[4], 1);
  cl_assert_equal_i(s_removed, 1);
}

void test_language_pack_protocol__rejects_selecting_font_only_pack(void) {
  s_present[1] = s_fonts[1] = true;
  const uint8_t request[] = {0, SysMsgLanguageSelect, 12, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  language_pack_protocol_handle_select(NULL, request, sizeof(request));
  cl_assert_equal_i(s_response[4], 0);
  cl_assert_equal_i(s_language, ShellLanguageEnglish);
}

void test_language_pack_protocol__reports_active_translation_slot(void) {
  s_present[3] = s_valid[3] = true;
  s_language = ShellLanguagePack3;
  const uint8_t request[] = {0, SysMsgLanguagePacks, 8};
  language_pack_protocol_handle_list(NULL, request, sizeof(request));
  cl_assert_equal_i(s_response[3], 3);
}

void test_language_pack_protocol__removes_matching_pack(void) {
  const uint8_t request[] = {0, SysMsgLanguagePackRemove, 9, 0, 0x34, 0x12, 'f', 'r', '_', 'F', 'R',
                             0};
  language_pack_protocol_handle_remove(NULL, request, sizeof(request));
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[2], 9);
  cl_assert_equal_i(s_response[3], 0);
  cl_assert_equal_i(s_response[4], 1);
  cl_assert_equal_i(s_removed, 0);
}

void test_language_pack_protocol__rejects_stale_metadata(void) {
  const uint8_t request[] = {0, SysMsgLanguagePackRemove, 9, 0, 1, 0, 'f', 'r', '_', 'F', 'R', 0};
  language_pack_protocol_handle_remove(NULL, request, sizeof(request));
  cl_assert_equal_i(s_response[4], 0);
  cl_assert_equal_i(s_removed, -1);
}

void test_language_pack_protocol__removes_with_future_trailing_fields(void) {
  const uint8_t request[] = {
    0, SysMsgLanguagePackRemove, 9, 0, 0x34, 0x12, 'f', 'r', '_', 'F', 'R', 0, 0xab, 0xcd
  };
  language_pack_protocol_handle_remove(NULL, request, sizeof(request));
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[1], SysMsgLanguagePackRemoveResponse);
  cl_assert_equal_i(s_response[2], 9);
  cl_assert_equal_i(s_response[4], 1);
  cl_assert_equal_i(s_removed, 0);
}

void test_language_pack_protocol__selects_with_future_trailing_fields(void) {
  const uint8_t request[] = {
    0, SysMsgLanguageSelect, 10, 0, 0x34, 0x12, 'f', 'r', '_', 'F', 'R', 0, 0xab, 0xcd
  };
  language_pack_protocol_handle_select(NULL, request, sizeof(request));
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[1], SysMsgLanguageSelectResponse);
  cl_assert_equal_i(s_response[2], 10);
  cl_assert_equal_i(s_response[4], 1);
  cl_assert_equal_i(s_language, ShellLanguageInstalledPack);
}

void test_language_pack_protocol__removes_invalid_pack(void) {
  const uint8_t request[] = {0, SysMsgLanguagePackRemove, 9, 2, 0, 0, 0, 0, 0, 0, 0, 0};
  language_pack_protocol_handle_remove(NULL, request, sizeof(request));
  cl_assert_equal_i(s_response[4], 1);
  cl_assert_equal_i(s_removed, 2);
}

void test_language_pack_protocol__answers_malformed_removal(void) {
  uint8_t request[] = {0, SysMsgLanguagePackRemove, 9, 4, 0, 0, 0, 0, 0, 0, 0, 0};
  language_pack_protocol_handle_remove(NULL, request, sizeof(request));
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[1], SysMsgLanguagePackRemoveResponse);
  cl_assert_equal_i(s_response[2], 9);
  cl_assert_equal_i(s_response[3], 4);
  cl_assert_equal_i(s_response[4], 0);
  request[3] = 0;
  s_length = 0;
  language_pack_protocol_handle_remove(NULL, request, sizeof(request) - 1);
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[4], 0);
  s_length = 0;
  language_pack_protocol_handle_remove(NULL, request, 2);
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[2], 0);
  cl_assert_equal_i(s_response[3], 0xff);
  cl_assert_equal_i(s_removed, -1);
}

void test_language_pack_protocol__lists_without_token(void) {
  const uint8_t request[] = {0, SysMsgLanguagePacks};
  language_pack_protocol_handle_list(NULL, request, sizeof(request));
  cl_assert_equal_i(s_length, 164);
  cl_assert_equal_i(s_response[2], 0);
}

void test_language_pack_protocol__selects_pack_without_removing_fonts(void) {
  s_present[3] = s_valid[3] = true;
  const uint8_t request[] = {0, SysMsgLanguageSelect, 10, 3, 0x34, 0x12, 'f', 'r', '_', 'F', 'R',
                             0};
  language_pack_protocol_handle_select(NULL, request, sizeof(request));
  cl_assert_equal_i(s_language, ShellLanguagePack3);
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[1], SysMsgLanguageSelectResponse);
  cl_assert_equal_i(s_response[2], 10);
  cl_assert_equal_i(s_response[3], 3);
  cl_assert_equal_i(s_response[4], 1);
  cl_assert(s_present[0] && s_present[2] && s_present[3]);
  cl_assert_equal_i(s_removed, -1);
}

void test_language_pack_protocol__selects_english_without_removing_fonts(void) {
  s_language = ShellLanguageInstalledPack;
  const uint8_t request[] = {0, SysMsgLanguageSelect, 11, 0xff, 0, 0, 0, 0, 0, 0, 0, 0};
  language_pack_protocol_handle_select(NULL, request, sizeof(request));
  cl_assert_equal_i(s_language, ShellLanguageEnglish);
  cl_assert_equal_i(s_response[4], 1);
  cl_assert(s_present[0] && s_present[2]);
}

void test_language_pack_protocol__rejects_invalid_and_stale_language_selection(void) {
  uint8_t request[] = {0, SysMsgLanguageSelect, 12, 0, 1, 0, 'f', 'r', '_', 'F', 'R', 0};
  language_pack_protocol_handle_select(NULL, request, sizeof(request));
  cl_assert_equal_i(s_language, ShellLanguageEnglish);
  cl_assert_equal_i(s_response[4], 0);
  memset(request + 4, 0, 8);
  request[3] = 2;
  language_pack_protocol_handle_select(NULL, request, sizeof(request));
  cl_assert_equal_i(s_response[4], 0);
  cl_assert_equal_i(s_language, ShellLanguageEnglish);
}

void test_language_pack_protocol__answers_malformed_language_selection(void) {
  uint8_t request[] = {0, SysMsgLanguageSelect, 13, 4, 0, 0, 0, 0, 0, 0, 0, 0};
  language_pack_protocol_handle_select(NULL, request, sizeof(request));
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[1], SysMsgLanguageSelectResponse);
  cl_assert_equal_i(s_response[4], 0);
  request[3] = 0xff;
  s_length = 0;
  language_pack_protocol_handle_select(NULL, request, sizeof(request) - 1);
  cl_assert_equal_i(s_length, 5);
  cl_assert_equal_i(s_response[2], 13);
  cl_assert_equal_i(s_response[4], 0);
  cl_assert_equal_i(s_language, ShellLanguageEnglish);
}
