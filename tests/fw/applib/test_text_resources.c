/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <fixtures/load_test_resources.h>

// FW headers
#include <pbl/util/size.h>

#include <applib/graphics/text_resources.h>
#include <resource/language_pack.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>
#include <resource/system_resource.h>

// Fakes
#include <fake_app_manager.h>
#include <fake_pfs.h>

// Stubs

#define WILDCARD_CODEPOINT 0x25AF

static FontCache s_font_cache;
static FontInfo s_font_info;

// The renderer obtains the per-glyph fallback font from fonts_get_fallback_font(), which calls
// sys_font_get_system_font(NULL). A test installs a fallback by pointing s_test_fallback_font at
// its own FontInfo (NULL = no fallback, the default). The strong override of the weak syscall stub
// lives in test_text_resources_font_stub.c so it resolves at link time without colliding with the
// in-TU stub.
extern FontInfo *s_test_fallback_font;
// Installed emoji font, returned for any named font key (NULL = none, the default).
extern FontInfo *s_test_emoji_font;
extern FontInfo *s_test_language_fonts[LANGUAGE_PACK_COUNT];
extern int s_test_language_font_calls;

#define FONT_COMPRESSION_FIXTURE_PATH "font_compression"

// Helpers
////////////////////////////////////

static size_t glyph_get_size_bytes(const GlyphData *glyph) {
  return ((glyph->header.width_px * glyph->header.height_px) + (8 - 1)) / 8;
}

void test_text_resources__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang");
  // cl_assert(resource_has_valid_system_resources());

  memset(&s_font_info, 0, sizeof(s_font_info));
  memset(&s_font_cache, 0, sizeof(s_font_cache));
  s_test_fallback_font = nullptr;
  s_test_emoji_font = nullptr;
  memset(s_test_language_fonts, 0, sizeof(FontInfo *) * LANGUAGE_PACK_COUNT);
  g_font_language_generation = 0;
  text_resources_reset_cache(&s_font_cache);

  resource_init();
}

void test_text_resources__cleanup(void) {
}

void test_text_resources__init_font(void) {
  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));
  cl_assert_equal_i(FONT_VERSION(s_font_info.base.md.version), 3);
  cl_assert_equal_i(s_font_info.base.md.wildcard_codepoint, WILDCARD_CODEPOINT);
  cl_assert_equal_i(s_font_info.base.md.codepoint_bytes, 2);
}

void test_text_resources__glyph_from_second_extra_pack(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang2");
  FontInfo latin = {}, chinese = {};
  cl_assert(text_resources_init_font(SYSTEM_APP, RESOURCE_ID_GOTHIC_18, 0, &latin));
  const uint32_t extra_resource =
      language_pack_resource_id(2, RESOURCE_ID_GOTHIC_18_EXTENDED, RESOURCE_ID_STRINGS);
  cl_assert(text_resources_init_font(SYSTEM_APP, extra_resource, 0, &chinese));
  s_test_language_fonts[1] = &latin;
  s_test_language_fonts[2] = &chinese;
  cl_assert(text_resources_init_font(SYSTEM_APP, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  GlyphLocation location;
  const GlyphData *glyph = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &location);
  cl_assert(glyph);
  cl_assert_equal_i(location.font_res->resource_id, extra_resource);
  cl_assert_equal_i(text_resources_get_glyph_horiz_advance(&s_font_cache, 0x4E50, &s_font_info),
                    glyph->header.horiz_advance);

  cl_assert(text_resources_get_glyph(&s_font_cache, 'a', &s_font_info, &location));
  cl_assert_equal_i(location.font_res->resource_id, RESOURCE_ID_GOTHIC_18);
}

void test_text_resources__extra_pack_change_invalidates_missing_glyph_cache(void) {
  cl_assert(text_resources_init_font(SYSTEM_APP, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));
  GlyphLocation location;
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &location));
  cl_assert_equal_i(location.font_res->resource_id, RESOURCE_ID_GOTHIC_18);

  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  const uint32_t extra_resource =
      language_pack_resource_id(1, RESOURCE_ID_GOTHIC_18_EXTENDED, RESOURCE_ID_STRINGS);
  FontInfo chinese = {};
  cl_assert(text_resources_init_font(SYSTEM_APP, extra_resource, 0, &chinese));
  s_test_language_fonts[1] = &chinese;
  ++g_font_language_generation;
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &location));
  cl_assert_equal_i(location.font_res->resource_id, extra_resource);

  cl_assert_equal_i(pbl_fs_unlink("/lang1"), 0);
  s_test_language_fonts[1] = NULL;
  ++g_font_language_generation;
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &location));
  cl_assert_equal_i(location.font_res->resource_id, RESOURCE_ID_GOTHIC_18);
}

void test_text_resources__extra_pack_fonts_are_fetched_once(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  FontInfo chinese = {};
  cl_assert(text_resources_init_font(
      SYSTEM_APP, language_pack_resource_id(1, RESOURCE_ID_GOTHIC_18_EXTENDED, RESOURCE_ID_STRINGS),
      0, &chinese));
  s_test_language_fonts[1] = &chinese;
  cl_assert(text_resources_init_font(SYSTEM_APP, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  // A glyph missing from every pack fetches each slot once
  s_test_language_font_calls = 0;
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x0E01, &s_font_info, NULL));
  cl_assert_equal_i(s_test_language_font_calls, LANGUAGE_PACK_COUNT);
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, NULL));
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, NULL));
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x0E01, &s_font_info, NULL));
  cl_assert_equal_i(s_test_language_font_calls, LANGUAGE_PACK_COUNT);

  ++g_font_language_generation;
  cl_assert(text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, NULL));
  cl_assert(s_test_language_font_calls > LANGUAGE_PACK_COUNT);
}

void test_text_resources__same_baseline_for_active_and_secondary_pack(void) {
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "/lang1");
  const uint32_t extension =
      language_pack_resource_id(1, RESOURCE_ID_GOTHIC_18_EXTENDED, RESOURCE_ID_STRINGS);
  cl_assert(text_resources_init_font(SYSTEM_APP, RESOURCE_ID_GOTHIC_36, extension, &s_font_info));
  cl_assert(s_font_info.extended);
  cl_assert(s_font_info.extension.md.max_height < s_font_info.base.md.max_height);

  GlyphLocation active;
  const GlyphData *glyph = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &active);
  cl_assert(glyph);
  cl_assert_equal_i(active.font_res->resource_id, extension);
  cl_assert_equal_i(active.baseline_adjust, 0);
  const GlyphHeaderData active_header = glyph->header;
  const size_t bitmap_size = glyph_get_size_bytes(glyph);
  uint8_t bitmap[CACHE_GLYPH_SIZE];
  cl_assert(bitmap_size <= sizeof(bitmap));
  memcpy(bitmap, glyph->data, bitmap_size);

  FontInfo chinese = {};
  cl_assert(text_resources_init_font(SYSTEM_APP, extension, 0, &chinese));
  s_test_language_fonts[1] = &chinese;
  cl_assert(text_resources_init_font(SYSTEM_APP, RESOURCE_ID_GOTHIC_36, 0, &s_font_info));
  ++g_font_language_generation;

  GlyphLocation secondary;
  glyph = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &secondary);
  cl_assert(glyph);
  cl_assert_equal_i(secondary.font_res->resource_id, extension);
  cl_assert_equal_m(&active_header, &glyph->header, sizeof(active_header));
  cl_assert_equal_m(bitmap, glyph->data, bitmap_size);
  cl_assert_equal_i(secondary.baseline_adjust, active.baseline_adjust);
}

void test_text_resources__horiz_advance(void) {
  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  int8_t horiz_advance = text_resources_get_glyph_horiz_advance(&s_font_cache, 'a', &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance,
                    text_resources_get_glyph_horiz_advance(&s_font_cache, 'a', &s_font_info));
  cl_assert_equal_i(horiz_advance,
                    text_resources_get_glyph_horiz_advance(&s_font_cache, 'a', &s_font_info));
}

void test_text_resources__horiz_advance_multiple(void) {
  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  int8_t horiz_advance = text_resources_get_glyph_horiz_advance(&s_font_cache, 'a', &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance,
                    text_resources_get_glyph_horiz_advance(&s_font_cache, 'a', &s_font_info));

  horiz_advance = text_resources_get_glyph_horiz_advance(&s_font_cache, 'b', &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance,
                    text_resources_get_glyph_horiz_advance(&s_font_cache, 'b', &s_font_info));

  horiz_advance = text_resources_get_glyph_horiz_advance(&s_font_cache, 'c', &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance,
                    text_resources_get_glyph_horiz_advance(&s_font_cache, 'c', &s_font_info));
}

void test_text_resources__get_glyph_multiple(void) {
  const uint8_t a_glyph_data_bytes[] = {0x2e, 0x42, 0x2e, 0x63, 0xb6};
  const uint8_t b_glyph_data_bytes[] = {0x21, 0x84, 0x36, 0x63, 0x8c, 0x71, 0x36};
  const uint8_t c_glyph_data_bytes[] = {0x2e, 0x86, 0x10, 0x42, 0x74};

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  size_t glyph_size_bytes;
  const GlyphData *glyph;

  glyph = text_resources_get_glyph(&s_font_cache, 'a', &s_font_info, nullptr);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(a_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 'b', &s_font_info, nullptr);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(b_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 'c', &s_font_info, nullptr);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(c_glyph_data_bytes, glyph->data, glyph_size_bytes);
}

void test_text_resources__glyph_frame(void) {
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  const GRect frame = text_resources_get_glyph_frame(&s_font_cache, '0', &s_font_info);
  const GlyphData *glyph = text_resources_get_glyph(&s_font_cache, '0', &s_font_info, nullptr);
  cl_assert_equal_i(frame.origin.x, glyph->header.left_offset_px);
  cl_assert_equal_i(frame.origin.y, glyph->header.top_offset_px);
  cl_assert_equal_i(frame.size.w, glyph->header.width_px);
  cl_assert_equal_i(frame.size.h, glyph->header.height_px);
  // A digit sits inside the line of text
  cl_assert(frame.size.h > 0);
  cl_assert(frame.origin.y > 0);
  cl_assert(frame.origin.y + frame.size.h <= s_font_info.max_height);
}

void test_text_resources__glyph_frame_of_missing_glyph(void) {
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  // A codepoint the font lacks draws the wildcard box, so it has the box's frame
  const GRect missing = text_resources_get_glyph_frame(&s_font_cache, 0x4E00, &s_font_info);
  const GRect wildcard = text_resources_get_glyph_frame(
      &s_font_cache, s_font_info.base.md.wildcard_codepoint, &s_font_info);
  cl_assert(missing.size.h > 0);
  cl_assert_equal_m(&missing, &wildcard, sizeof(GRect));
}

static void prv_assert_same_glyph(Codepoint styled, Codepoint plain) {
  uint8_t expected[64];
  const GlyphData *glyph = text_resources_get_glyph(&s_font_cache, plain, &s_font_info, nullptr);
  const size_t size = glyph_get_size_bytes(glyph);
  cl_assert(size <= sizeof(expected));
  memcpy(expected, glyph->data, size);

  glyph = text_resources_get_glyph(&s_font_cache, styled, &s_font_info, nullptr);
  cl_assert_equal_i(glyph_get_size_bytes(glyph), size);
  cl_assert_equal_m(expected, glyph->data, size);
}

void test_text_resources__styled_latin_draws_plain_glyph(void) {
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  // Mathematical bold, monospace and double-struck digits
  prv_assert_same_glyph(0x1d400, 'A');
  prv_assert_same_glyph(0x1d41a, 'a');
  prv_assert_same_glyph(0x1d670, 'A');
  prv_assert_same_glyph(0x1d689, 'Z');
  prv_assert_same_glyph(0x1d68a, 'a');
  prv_assert_same_glyph(0x1d6a3, 'z');
  prv_assert_same_glyph(0x1d7ce, '0');
  prv_assert_same_glyph(0x1d7ff, '9');
  // Letterlike symbols
  prv_assert_same_glyph(0x212c, 'B');
  prv_assert_same_glyph(0x212b, 0xc5);
  prv_assert_same_glyph(0x210e, 'h');
  // Fullwidth forms
  prv_assert_same_glyph(0xff01, '!');
  prv_assert_same_glyph(0xff21, 'A');
  prv_assert_same_glyph(0xff5e, '~');
}

void test_text_resources__styled_greek_draws_wildcard(void) {
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  // Mathematical bold capital alpha
  prv_assert_same_glyph(0x1d6a8, s_font_info.base.md.wildcard_codepoint);
}

void test_text_resources__init_backup_font(void) {
  // load the built in fallback font
  uint32_t font_fallback = RESOURCE_ID_FONT_FALLBACK_INTERNAL;
  cl_assert(text_resources_init_font(0, font_fallback, 0, &s_font_info));
  cl_assert_equal_i(FONT_VERSION(s_font_info.base.md.version), 3);
  cl_assert_equal_i(s_font_info.base.md.wildcard_codepoint, WILDCARD_CODEPOINT);
}

void test_text_resources__test_backup_wildcard(void) {
  const uint8_t wildcard_bytes[] = {0x3f, 0xc6, 0x18, 0x63, 0x8c, 0x31, 0xc6, 0x0f};

  uint32_t font_fallback = RESOURCE_ID_FONT_FALLBACK_INTERNAL;
  cl_assert(text_resources_init_font(0, font_fallback, 0, &s_font_info));

  int8_t horiz_advance =
      text_resources_get_glyph_horiz_advance(&s_font_cache, WILDCARD_CODEPOINT, &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance, text_resources_get_glyph_horiz_advance(
                                       &s_font_cache, WILDCARD_CODEPOINT, &s_font_info));

  const GlyphData *glyph =
      text_resources_get_glyph(&s_font_cache, WILDCARD_CODEPOINT, &s_font_info, nullptr);
  cl_assert_equal_i(glyph->header.width_px, 5);
  cl_assert_equal_i(glyph->header.height_px, 12);
  size_t glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(wildcard_bytes, glyph->data, glyph_size_bytes);
}

void test_text_resources__test_gothic_wildcard(void) {
  uint8_t wildcard_bytes[] = {0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83,
                              0xc1, 0x60, 0x30, 0x18, 0x0c, 0xfe, 0x01};

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  int8_t horiz_advance =
      text_resources_get_glyph_horiz_advance(&s_font_cache, WILDCARD_CODEPOINT, &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance, text_resources_get_glyph_horiz_advance(
                                       &s_font_cache, WILDCARD_CODEPOINT, &s_font_info));

  const GlyphData *glyph =
      text_resources_get_glyph(&s_font_cache, WILDCARD_CODEPOINT, &s_font_info, nullptr);
  cl_assert_equal_i(glyph->header.width_px, 7);
  cl_assert_equal_i(glyph->header.height_px, 15);
  size_t glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(wildcard_bytes, glyph->data, glyph_size_bytes);
}

void test_text_resources__extended_font(void) {
  const uint8_t chinese_wildcard_bytes[] = {0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83,
                                            0xc1, 0x60, 0x30, 0x18, 0x0c, 0xfe, 0x01};
  const uint8_t a_glyph_data_bytes[] = {0x2e, 0x42, 0x2e, 0x63, 0xb6};
  const uint8_t chinese_glyph_data_bytes[] = {0x00, 0x0C, 0xE2, 0x01, 0x0F, 0x80, 0x30, 0x40,
                                              0x08, 0x10, 0x04, 0x08, 0x82, 0xFC, 0xFF, 0x80,
                                              0x00, 0x44, 0x00, 0x26, 0x01, 0x11, 0x41, 0x08,
                                              0x11, 0x84, 0x04, 0x82, 0xC0, 0x01, 0x40, 0x00};

  uint32_t gothic_18_bold_handle = RESOURCE_ID_GOTHIC_18;
  uint32_t gothic_18_bold_extended_handle = RESOURCE_ID_GOTHIC_18_EXTENDED;
  cl_assert(text_resources_init_font(0, gothic_18_bold_handle, gothic_18_bold_extended_handle,
                                     &s_font_info));
  cl_assert(s_font_info.loaded);
  cl_assert(s_font_info.extended);

  size_t glyph_size_bytes;
  const GlyphData *glyph;

  glyph = text_resources_get_glyph(&s_font_cache, 'a', &s_font_info, nullptr);
  cl_assert(glyph != nullptr);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(a_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 0x4E50 /* 乐 */, &s_font_info, nullptr);
  // the chinese pbpack contains the letter 你, it should succeed
  cl_assert(glyph != nullptr);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(chinese_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 0x8888 /* 袈 */, &s_font_info, nullptr);
  // the chinese pbpack does not contain the letter 袈, it should return the wildcard
  cl_assert(glyph != nullptr);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(chinese_wildcard_bytes, glyph->data, glyph_size_bytes);
}

void test_text_resources__test_emoji_font(void) {
  uint32_t gothic_18_emoji_handle = RESOURCE_ID_GOTHIC_18_EMOJI;
  cl_assert(text_resources_init_font(0, gothic_18_emoji_handle, 0, &s_font_info));
  // Color glyph bodies aren't cached, so only the header can be checked
  cl_assert(HAS_FEATURE(s_font_info.base.md.version, VERSION_FIELD_FEATURE_COLOR));

  const Codepoint PHONE_CODEPOINT = 0x260E;
  const GlyphData *glyph =
      text_resources_get_glyph(&s_font_cache, PHONE_CODEPOINT, &s_font_info, nullptr);
  cl_assert(glyph != nullptr);
  cl_assert_equal_i(glyph->header.width_px, 8);
  cl_assert_equal_i(glyph->header.height_px, 9);
  cl_assert_equal_i(glyph->header.horiz_advance, 10);
}

void DISABLED_test_text_resources__test_emoji_fallback(void) {
  const uint8_t phone_bytes[] = {0xfe, 0x81, 0x81, 0x3c, 0x66, 0x42,
                                 0xc3, 0xe7, 0xff, 0x00, 0x00, 0x00};

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  size_t glyph_size_bytes;
  const GlyphData *glyph;

  const Codepoint PHONE_CODEPOINT = 0x260E;
  glyph = text_resources_get_glyph(&s_font_cache, PHONE_CODEPOINT, &s_font_info, nullptr);
  cl_assert(glyph != nullptr);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(phone_bytes, glyph->data, glyph_size_bytes);
}

// Per-glyph fallback chain
////////////////////////////////////

// Core regression: a codepoint absent from the primary font but present in the configured
// fallback font now resolves to the real fallback glyph, instead of silently yielding only the
// primary wildcard box.
void test_text_resources__per_glyph_fallback(void) {
  // primary: plain gothic 18, no extension -> lacks CJK 0x4E50
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  // Baseline with no fallback: 0x4E50 misses -> gothic wildcard, NOT the CJK glyph.
  const uint8_t gothic_wildcard[] = {0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83,
                                     0xc1, 0x60, 0x30, 0x18, 0x0c, 0xfe, 0x01};
  const GlyphData *g0 = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, nullptr);
  cl_assert(g0 != nullptr);
  cl_assert_equal_m(gothic_wildcard, g0->data, glyph_get_size_bytes(g0));

  // Install fallback = gothic 18 + extended (which contains 0x4E50) as the system fallback font.
  static FontInfo s_fallback;
  memset(&s_fallback, 0, sizeof(s_fallback));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, RESOURCE_ID_GOTHIC_18_EXTENDED,
                                     &s_fallback));
  s_test_fallback_font = &s_fallback;

  // Fresh cache so the primary negative-cache entry from g0 does not short-circuit.
  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  const uint8_t cjk_bytes[] = {0x00, 0x0C, 0xE2, 0x01, 0x0F, 0x80, 0x30, 0x40, 0x08, 0x10, 0x04,
                               0x08, 0x82, 0xFC, 0xFF, 0x80, 0x00, 0x44, 0x00, 0x26, 0x01, 0x11,
                               0x41, 0x08, 0x11, 0x84, 0x04, 0x82, 0xC0, 0x01, 0x40, 0x00};
  const GlyphData *g1 = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, nullptr);
  cl_assert(g1 != nullptr);
  cl_assert_equal_m(cjk_bytes, g1->data, glyph_get_size_bytes(g1)); // real fallback glyph
}

// A glyph served by the system fallback font reports a positive baseline adjust, so the renderer
// can drop it onto the primary font's baseline instead of the fallback font's.
void test_text_resources__baseline_adjust_for_fallback_font(void) {
  // primary: gothic 24, no extension -> lacks CJK 0x4E50
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_24, 0, &s_font_info));

  // A glyph from the primary font itself needs no adjust.
  GlyphLocation loc = {.baseline_adjust = -1};
  const GlyphData *g0 = text_resources_get_glyph(&s_font_cache, 'a', &s_font_info, &loc);
  cl_assert(g0 != nullptr);
  cl_assert_equal_i(loc.baseline_adjust, 0);

  // Install a shorter fallback (gothic 18 + extended) that carries 0x4E50.
  static FontInfo s_fallback;
  memset(&s_fallback, 0, sizeof(s_fallback));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, RESOURCE_ID_GOTHIC_18_EXTENDED,
                                     &s_fallback));
  s_test_fallback_font = &s_fallback;

  // Fresh cache so the primary negative-cache entry does not short-circuit.
  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  // The reference is the fallback font's own baseline, i.e. its BASE resource height.
  const int16_t expected =
      (int16_t)s_font_info.base.md.max_height - (int16_t)s_fallback.base.md.max_height;
  cl_assert(expected > 0); // premise: the fallback really is shorter

  loc.baseline_adjust = -1;
  const GlyphData *g1 = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &loc);
  cl_assert(g1 != nullptr);
  cl_assert_equal_i(loc.baseline_adjust, expected);
}

// Regression: an extension is part of the SAME font and its glyph offsets are already baked
// against that font's base baseline, so a short extension (as shipped for ar_SA, where a 36px
// font carries a 20px-tall extension) must never be pushed down.
void test_text_resources__baseline_adjust_zero_for_own_extension(void) {
  // gothic 36 base with a much shorter extension, mimicking the ar_SA font pack
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_36, RESOURCE_ID_GOTHIC_18_EXTENDED,
                                     &s_font_info));
  cl_assert(s_font_info.extended);
  // premise: the extension really is shorter than the base
  cl_assert(s_font_info.extension.md.max_height < s_font_info.base.md.max_height);

  // Compare against the extension's own glyph bytes, so a wildcard from the base cannot pass.
  const uint8_t cjk_bytes[] = {0x00, 0x0C, 0xE2, 0x01, 0x0F, 0x80, 0x30, 0x40, 0x08, 0x10, 0x04,
                               0x08, 0x82, 0xFC, 0xFF, 0x80, 0x00, 0x44, 0x00, 0x26, 0x01, 0x11,
                               0x41, 0x08, 0x11, 0x84, 0x04, 0x82, 0xC0, 0x01, 0x40, 0x00};
  GlyphLocation loc = {.baseline_adjust = -1};
  const GlyphData *g = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, &loc);
  cl_assert(g != nullptr);
  cl_assert_equal_m(cjk_bytes, g->data, glyph_get_size_bytes(g));
  cl_assert_equal_i(loc.baseline_adjust, 0);
}

// The case this all exists for: a 36px font has no emoji font of its own size, so the 28px one
// takes over and its glyphs must be dropped 8px to reach the 36px baseline.
void test_text_resources__baseline_adjust_for_emoji_font(void) {
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_36, 0, &s_font_info));
  cl_assert_equal_i(s_font_info.base.md.max_height, 36);

  // fonts_get_system_emoji_font_for_size(36) picks the 28px emoji font, which this stub serves.
  static FontInfo s_emoji;
  memset(&s_emoji, 0, sizeof(s_emoji));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_28_EMOJI, 0, &s_emoji));
  cl_assert_equal_i(s_emoji.base.md.max_height, 28);
  s_test_emoji_font = &s_emoji;

  const Codepoint PHONE_CODEPOINT = 0x260E;
  GlyphLocation loc = {.baseline_adjust = -1};
  const GlyphData *g = text_resources_get_glyph(&s_font_cache, PHONE_CODEPOINT, &s_font_info, &loc);
  cl_assert(g != nullptr);
  cl_assert_equal_i(loc.baseline_adjust, 8); // 36px primary baseline - 28px emoji font baseline
}

// A codepoint present in the primary font is served by the primary font; the fallback is not
// consulted. Wire a different fallback whose 'a' glyph provably differs from the primary's (a
// smaller font has different glyph dimensions and therefore different bitmap bytes), capture both
// dynamically, assert the premise, then verify the result equals the PRIMARY bytes.
void test_text_resources__fallback_not_used_when_present(void) {
  // Step 1: capture the primary (GOTHIC_18) 'a' bytes.
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));
  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  const GlyphData *primary_g = text_resources_get_glyph(&s_font_cache, 'a', &s_font_info, nullptr);
  cl_assert(primary_g != nullptr);
  size_t primary_size = glyph_get_size_bytes(primary_g);

  // Copy the primary glyph bitmap so we have a stable snapshot even after a cache reset.
  uint8_t primary_bytes[CACHE_GLYPH_SIZE];
  cl_assert(primary_size <= sizeof(primary_bytes));
  memcpy(primary_bytes, primary_g->data, primary_size);

  // Step 2: init GOTHIC_14 as the fallback and fetch its 'a' directly.
  // GOTHIC_14 is shorter (14px cap-height vs 18px), so its 'a' bitmap has different dimensions
  // and therefore different bytes — the premise check in step 3 will catch any regression where
  // they happen to collide.
  static FontInfo s_fallback;
  memset(&s_fallback, 0, sizeof(s_fallback));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_14, 0, &s_fallback));

  // Fresh cache so there is no stale entry from the primary fetch above.
  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  const GlyphData *fallback_g = text_resources_get_glyph(&s_font_cache, 'a', &s_fallback, nullptr);
  cl_assert(fallback_g != nullptr);
  size_t fallback_size = glyph_get_size_bytes(fallback_g);

  uint8_t fallback_bytes[CACHE_GLYPH_SIZE];
  cl_assert(fallback_size <= sizeof(fallback_bytes));
  memcpy(fallback_bytes, fallback_g->data, fallback_size);

  // Step 3: assert the premise — the fallback 'a' must differ from the primary 'a'.
  // If it doesn't, this test cannot distinguish "primary used" from "fallback used".
  cl_assert(primary_size != fallback_size ||
            memcmp(primary_bytes, fallback_bytes, primary_size) != 0);

  // Step 4: install the fallback, reset the cache, and fetch 'a' through the primary font.
  // The result must equal the CAPTURED PRIMARY bytes, proving the fallback was not consulted.
  s_test_fallback_font = &s_fallback;
  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  const GlyphData *result_g = text_resources_get_glyph(&s_font_cache, 'a', &s_font_info, nullptr);
  cl_assert(result_g != nullptr);
  cl_assert_equal_i(glyph_get_size_bytes(result_g), primary_size);
  cl_assert_equal_m(primary_bytes, result_g->data, primary_size);
}

// When the primary font IS the system fallback font, the self-reference guard skips the redundant
// second lookup: no loop or crash, and a missing codepoint still yields the primary wildcard.
void test_text_resources__fallback_self_reference(void) {
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));
  s_test_fallback_font = &s_font_info; // primary is its own fallback

  const uint8_t gothic_wildcard[] = {0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83,
                                     0xc1, 0x60, 0x30, 0x18, 0x0c, 0xfe, 0x01};
  const GlyphData *g = text_resources_get_glyph(&s_font_cache, 0x4E50, &s_font_info, nullptr);
  cl_assert(g != nullptr); // returns primary wildcard, no hang
  cl_assert_equal_m(gothic_wildcard, g->data, glyph_get_size_bytes(g));
}

// A codepoint missing from BOTH primary and fallback still yields the primary wildcard,
// proving the chain-exhausted tail survives.
void test_text_resources__fallback_miss_yields_primary_wildcard(void) {
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));
  static FontInfo s_fallback;
  memset(&s_fallback, 0, sizeof(s_fallback));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_fallback));
  s_test_fallback_font = &s_fallback;

  const uint8_t gothic_wildcard[] = {0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83,
                                     0xc1, 0x60, 0x30, 0x18, 0x0c, 0xfe, 0x01};
  const GlyphData *g =
      text_resources_get_glyph(&s_font_cache, 0x8888 /* absent */, &s_font_info, nullptr);
  cl_assert(g != nullptr);
  cl_assert_equal_m(gothic_wildcard, g->data, glyph_get_size_bytes(g));
}

// Offset table lookups
////////////////////////////////////

static void prv_reset_font_cache(void) {
  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);
}

// Looks up codepoint in font after the same codepoint was looked up in other_font, which leaves
// other_font's offset table bucket in the shared buffer. The result must match a lookup done on a
// fresh cache.
static void prv_assert_lookup_ignores_other_font(uint32_t font, uint32_t other_font,
                                                 Codepoint codepoint) {
  static FontInfo s_other;
  memset(&s_other, 0, sizeof(s_other));
  cl_assert(text_resources_init_font(0, font, 0, &s_font_info));
  cl_assert(text_resources_init_font(0, other_font, 0, &s_other));

  uint8_t expected[sizeof(GlyphHeaderData) + CACHE_GLYPH_SIZE];
  const GlyphData *g = text_resources_get_glyph(&s_font_cache, codepoint, &s_font_info, nullptr);
  const size_t expected_size = g ? sizeof(GlyphHeaderData) + glyph_get_size_bytes(g) : 0;
  memcpy(expected, g, expected_size);

  prv_reset_font_cache();
  cl_assert(text_resources_get_glyph(&s_font_cache, codepoint, &s_other, nullptr) != nullptr);
  g = text_resources_get_glyph(&s_font_cache, codepoint, &s_font_info, nullptr);
  cl_assert_equal_b(g != nullptr, expected_size != 0);
  cl_assert_equal_m(expected, g, expected_size);
}

// The hash bucket of U+00E4 is empty in LECO_60_NUMBERS_AM_PM, U+00E4 is first in it in
// ROBOTO_CONDENSED_21, and its offset there points at LECO bitmap bytes
void test_text_resources__empty_bucket_ignores_stale_offsets(void) {
  prv_assert_lookup_ignores_other_font(RESOURCE_ID_LECO_60_NUMBERS_AM_PM,
                                       RESOURCE_ID_ROBOTO_CONDENSED_21, 0x00E4);
}

// The hash bucket of U+0176 holds one lower codepoint in LECO_60_BOLD_NUMBERS_AM_PM, U+0176 is
// second in it in GOTHIC_36_BOLD, and its offset there points at LECO bitmap bytes
void test_text_resources__bucket_end_ignores_stale_offsets(void) {
  prv_assert_lookup_ignores_other_font(RESOURCE_ID_LECO_60_BOLD_NUMBERS_AM_PM,
                                       RESOURCE_ID_GOTHIC_36_BOLD, 0x0176);
}

// Glyph-presence-driven routing (in-font base<->extension rescue)
////////////////////////////////////

// Mirror of #1709: a codepoint whose Unicode class routes it to the EXTENSION but which lives only
// in the BASE (e.g. pi U+03C0) must still resolve to the base glyph once an extension is loaded,
// instead of degrading to the fallback/wildcard. Works on the shipped fixtures: GOTHIC_18 base
// carries U+03C0, the CJK extension does not.
void test_text_resources__extension_routed_miss_rescued_from_base(void) {
  const Codepoint PI = 0x03C0;

  // Reference: base-only GOTHIC_18 resolves U+03C0 to its own glyph (no extension, so it routes to
  // base directly).
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));
  const GlyphData *base_g = text_resources_get_glyph(&s_font_cache, PI, &s_font_info, nullptr);
  cl_assert(base_g != nullptr);
  size_t base_size = glyph_get_size_bytes(base_g);
  uint8_t base_bytes[CACHE_GLYPH_SIZE];
  cl_assert(base_size <= sizeof(base_bytes));
  memcpy(base_bytes, base_g->data, base_size);

  // Now load GOTHIC_18 + CJK extension. U+03C0 is not latin-classified, so it routes to the
  // extension, which lacks it. Without the in-font rescue this misses and yields the wildcard.
  memset(&s_font_info, 0, sizeof(s_font_info));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, RESOURCE_ID_GOTHIC_18_EXTENDED,
                                     &s_font_info));
  cl_assert(s_font_info.extended);

  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  GlyphLocation loc = {.baseline_adjust = -1};
  const GlyphData *g = text_resources_get_glyph(&s_font_cache, PI, &s_font_info, &loc);
  cl_assert(g != nullptr);
  cl_assert_equal_i(glyph_get_size_bytes(g), base_size);
  cl_assert_equal_m(base_bytes, g->data, base_size); // rescued from base, not the wildcard
  cl_assert_equal_i(loc.baseline_adjust, 0);         // owner is the primary font
}

// #1709 itself: a latin-classified codepoint absent from the base but present in the extension must
// resolve from the extension. The shipped GOTHIC_18 base already carries the extension's only latin
// codepoint (U+2026), so we pair the extension with a base font that lacks it — GOTHIC_18_EMOJI —
// which faithfully exercises the base-routed -> extension rescue path.
void test_text_resources__base_routed_miss_rescued_from_extension(void) {
  const Codepoint ELLIPSIS = 0x2026;

  // Reference: the extension's own U+2026 glyph, captured by loading the extension as a base font.
  static FontInfo s_probe;
  memset(&s_probe, 0, sizeof(s_probe));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18_EXTENDED, 0, &s_probe));
  const GlyphData *ext_g = text_resources_get_glyph(&s_font_cache, ELLIPSIS, &s_probe, nullptr);
  cl_assert(ext_g != nullptr);
  size_t ext_size = glyph_get_size_bytes(ext_g);
  uint8_t ext_bytes[CACHE_GLYPH_SIZE];
  cl_assert(ext_size <= sizeof(ext_bytes));
  memcpy(ext_bytes, ext_g->data, ext_size);

  // Base = emoji font (lacks U+2026) + CJK extension (has U+2026). U+2026 is latin-classified, so
  // it routes to base, misses, and must be rescued from the extension.
  memset(&s_font_info, 0, sizeof(s_font_info));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18_EMOJI, RESOURCE_ID_GOTHIC_18_EXTENDED,
                                     &s_font_info));
  cl_assert(s_font_info.extended);

  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  GlyphLocation loc = {.baseline_adjust = -1};
  const GlyphData *g = text_resources_get_glyph(&s_font_cache, ELLIPSIS, &s_font_info, &loc);
  cl_assert(g != nullptr);
  cl_assert_equal_i(glyph_get_size_bytes(g), ext_size);
  cl_assert_equal_m(ext_bytes, g->data, ext_size); // rescued from extension, not the wildcard
  cl_assert_equal_i(loc.baseline_adjust, 0);       // owner is the primary font
}

// Regression guards for the in-font rescue: (a) a codepoint present in the routed resource still
// wins over the other resource, and (b) a codepoint absent from both still yields the wildcard.
void test_text_resources__in_font_rescue_regressions(void) {
  const Codepoint ELLIPSIS = 0x2026;
  const Codepoint ABSENT = 0x8888; // absent from base, extension and fallback

  // Base-only references for U+2026 (base carries its own ellipsis) and the wildcard for U+8888.
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, 0, &s_font_info));

  const GlyphData *base_ellipsis =
      text_resources_get_glyph(&s_font_cache, ELLIPSIS, &s_font_info, nullptr);
  cl_assert(base_ellipsis != nullptr);
  size_t base_ellipsis_size = glyph_get_size_bytes(base_ellipsis);
  uint8_t base_ellipsis_bytes[CACHE_GLYPH_SIZE];
  cl_assert(base_ellipsis_size <= sizeof(base_ellipsis_bytes));
  memcpy(base_ellipsis_bytes, base_ellipsis->data, base_ellipsis_size);

  const GlyphData *base_wildcard =
      text_resources_get_glyph(&s_font_cache, ABSENT, &s_font_info, nullptr);
  cl_assert(base_wildcard != nullptr);
  size_t wildcard_size = glyph_get_size_bytes(base_wildcard);
  uint8_t wildcard_bytes[CACHE_GLYPH_SIZE];
  cl_assert(wildcard_size <= sizeof(wildcard_bytes));
  memcpy(wildcard_bytes, base_wildcard->data, wildcard_size);

  // Load GOTHIC_18 + CJK extension. Both base and extension carry U+2026 (with different bitmaps),
  // so the routed resource (base) must win.
  memset(&s_font_info, 0, sizeof(s_font_info));
  cl_assert(text_resources_init_font(0, RESOURCE_ID_GOTHIC_18, RESOURCE_ID_GOTHIC_18_EXTENDED,
                                     &s_font_info));
  cl_assert(s_font_info.extended);

  memset(&s_font_cache, 0, sizeof(s_font_cache));
  keyed_circular_cache_init(&s_font_cache.line_cache, s_font_cache.cache_keys,
                            s_font_cache.cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

  // (a) routed resource wins: U+2026 resolves to the BASE bytes, not the extension's.
  const GlyphData *g_ellipsis =
      text_resources_get_glyph(&s_font_cache, ELLIPSIS, &s_font_info, nullptr);
  cl_assert(g_ellipsis != nullptr);
  cl_assert_equal_i(glyph_get_size_bytes(g_ellipsis), base_ellipsis_size);
  cl_assert_equal_m(base_ellipsis_bytes, g_ellipsis->data, base_ellipsis_size);

  // (b) absent everywhere still yields the primary wildcard.
  const GlyphData *g_absent =
      text_resources_get_glyph(&s_font_cache, ABSENT, &s_font_info, nullptr);
  cl_assert(g_absent != nullptr);
  cl_assert_equal_i(glyph_get_size_bytes(g_absent), wildcard_size);
  cl_assert_equal_m(wildcard_bytes, g_absent->data, wildcard_size);
}

void test_text_resources__test_glyph_decompression(void) {
  // Create a second FontInfo for the compressed font.
  // The uncompressed font will use the global.
  FontInfo font_info_compressed;
  memset(&font_info_compressed, 0, sizeof(font_info_compressed));

  // Load GOTHIC_18
  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));
  cl_assert_equal_i(FONT_VERSION(s_font_info.base.md.version), 3);
  cl_assert(!HAS_FEATURE(s_font_info.base.md.version, VERSION_FIELD_FEATURE_RLE4));

  // Load GOTHIC_18_COMPRESSED. This is the same font, added by hand to the system resource pack.
  // To do this, simply copy the GOTHIC_18 stanza in resource/normal/base/resource_map.json, change
  // the name to include _COMPRESSED, and add the field: "compress": "RLE4". Rebuild, and run
  // ./tools/update_system_pbpack.sh
  // GOTHIC_18_COMPRESSED isn't in the regular pbpack; it has to be added by hand (see comment
  // above) and the resource_ids.auto.h override updated to give it a real ID. When it's absent
  // the symbol isn't defined, so skip the decompression coverage entirely.
#ifdef RESOURCE_ID_GOTHIC_18_COMPRESSED
  // There is no way to get the list of glyphs present in a font with the existing API. This list
  // of ranges lists the 371 glyphs currently in fontname.ttf
  typedef struct Codepoint_Range {
    uint16_t start;
    uint16_t end;
  } CodePoint_Range;
  const CodePoint_Range codepoint_range[] = {
    {0x0020, 0x007E}, {0x00A0, 0x00AC}, {0x00AE, 0x00D6}, {0x00D9, 0x017F}, {0x0192, 0x0192},
    {0x01FC, 0x01FF}, {0x0218, 0x021B}, {0x02C6, 0x02DD}, {0x03C0, 0x03C0}, {0x2013, 0x2014},
    {0x2018, 0x201A}, {0x201C, 0x201E}, {0x2020, 0x2022}, {0x2026, 0x2026}, {0x2030, 0x2030},
    {0x2039, 0x203A}, {0x2044, 0x2044}, {0x20AC, 0x20AC}, {0x2122, 0x2122}, {0x2126, 0x2126},
    {0x2202, 0x2202}, {0x2206, 0x2206}, {0x220F, 0x220F}, {0x2211, 0x2212}, {0x221A, 0x221A},
    {0x221E, 0x221E}, {0x222B, 0x222B}, {0x2248, 0x2248}, {0x2260, 0x2260}, {0x2264, 0x2265},
    {0x25AF, 0x25AF}, {0x25CA, 0x25CA}, {0xF6C3, 0xF6C3}, {0xFB01, 0xFB02}
  };

  uint32_t gothic_18_compressed_handle = RESOURCE_ID_GOTHIC_18_COMPRESSED;
  if (gothic_18_compressed_handle == INVALID_RESOURCE) {
    return;
  }
  cl_assert(text_resources_init_font(0, gothic_18_compressed_handle, 0, &font_info_compressed));
  cl_assert_equal_i(FONT_VERSION(font_info_compressed.base.md.version), 3);
  cl_assert(HAS_FEATURE(font_info_compressed.base.md.version, VERSION_FIELD_FEATURE_RLE4));

  // For each glyph in the font, get both the compressed and uncompressed bit field & header, and
  // assert that they are identical (ignoring any possible garbage after the bitmap).
  // Load the uncompressed glyph into this local glyph_buffer, and use the font cache for the
  // compressed glyph.
  uint8_t glyph_buffer[sizeof(GlyphHeaderData) + CACHE_GLYPH_SIZE];
  for (unsigned index = 0; index < ARRAY_LENGTH(codepoint_range); ++index) {
    for (unsigned codepoint = codepoint_range[index].start; codepoint <= codepoint_range[index].end;
         ++codepoint) {
      const GlyphData *glyph =
          text_resources_get_glyph(&s_font_cache, codepoint, &s_font_info, nullptr);
      cl_assert(glyph);

      unsigned glyph_size = sizeof(GlyphHeaderData) + glyph_get_size_bytes(glyph);
      memcpy(glyph_buffer, glyph->data, glyph_size);

      glyph = text_resources_get_glyph(&s_font_cache, codepoint, &font_info_compressed, nullptr);
      cl_assert(glyph);

      cl_assert_equal_m(glyph->data, glyph_buffer, glyph_size);
    }
  }
#endif
}
