/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "fixtures/load_test_resources.h"

// FW headers
#include "applib/graphics/text_resources.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "resource/system_resource.h"
#include "util/size.h"

// Fakes
#include "fake_app_manager.h"

// Stubs
#include "stubs_analytics.h"
#include "stubs_bootbits.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_passert.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_queue.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_prompt.h"
#include "stubs_task_watchdog.h"
#include "stubs_memory_layout.h"

#define WILDCARD_CODEPOINT 0x25AF

static FontCache s_font_cache;
static FontInfo s_font_info;

#define FONT_COMPRESSION_FIXTURE_PATH "font_compression"

// Helpers
////////////////////////////////////

static uint8_t glyph_get_size_bytes(const GlyphData *glyph) {
  return ((glyph->header.width_px * glyph->header.height_px) + (8 - 1)) / 8;
}

void test_text_resources__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false /* is_next */);
  load_resource_fixture_on_pfs(RESOURCES_FIXTURE_PATH, CHINESE_FIXTURE_NAME, "lang");
  //cl_assert(resource_has_valid_system_resources());

  memset(&s_font_info, 0, sizeof(s_font_info));
  memset(&s_font_cache, 0, sizeof(s_font_cache));

  FontCache *font_cache = &s_font_cache;
  memset(font_cache->cache_keys, 0, sizeof(font_cache->cache_keys));
  memset(font_cache->cache_data, 0, sizeof(font_cache->cache_data));
  keyed_circular_cache_init(&font_cache->line_cache, font_cache->cache_keys,
                            font_cache->cache_data, sizeof(LineCacheData), LINE_CACHE_SIZE);

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

  uint8_t glyph_size_bytes;
  const GlyphData *glyph;

  glyph = text_resources_get_glyph(&s_font_cache, 'a', &s_font_info);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(a_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 'b', &s_font_info);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(b_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 'c', &s_font_info);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(c_glyph_data_bytes, glyph->data, glyph_size_bytes);
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

  int8_t horiz_advance = text_resources_get_glyph_horiz_advance(&s_font_cache,
                                                                WILDCARD_CODEPOINT, &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance,
      text_resources_get_glyph_horiz_advance(&s_font_cache, WILDCARD_CODEPOINT, &s_font_info));

  const GlyphData *glyph = text_resources_get_glyph(&s_font_cache,
                                                    WILDCARD_CODEPOINT, &s_font_info);
  cl_assert_equal_i(glyph->header.width_px, 5);
  cl_assert_equal_i(glyph->header.height_px, 12);
  uint8_t glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(wildcard_bytes, glyph->data, glyph_size_bytes);
}

void test_text_resources__test_gothic_wildcard(void) {
  uint8_t wildcard_bytes[] = {0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83, 0xc1, 0x60, 0x30, 0x18, 0x0c, 0xfe, 0x01};

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  int8_t horiz_advance = text_resources_get_glyph_horiz_advance(&s_font_cache,
                                                                WILDCARD_CODEPOINT,
                                                                &s_font_info);
  cl_assert(horiz_advance != 0);
  cl_assert_equal_i(horiz_advance,
      text_resources_get_glyph_horiz_advance(&s_font_cache, WILDCARD_CODEPOINT, &s_font_info));

  const GlyphData *glyph = text_resources_get_glyph(&s_font_cache,
                                                    WILDCARD_CODEPOINT,
                                                    &s_font_info);
  cl_assert_equal_i(glyph->header.width_px, 7);
  cl_assert_equal_i(glyph->header.height_px, 15);
  uint8_t glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(wildcard_bytes, glyph->data, glyph_size_bytes);
}

void test_text_resources__extended_font(void) {
  const uint8_t chinese_wildcard_bytes[] = {0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x83, 0xc1, 0x60, 0x30, 0x18, 0x0c, 0xfe, 0x01};
  const uint8_t a_glyph_data_bytes[] = {0x2e, 0x42, 0x2e, 0x63, 0xb6};
  const uint8_t chinese_glyph_data_bytes[] = {0x00, 0x0C, 0xE2, 0x01, 0x0F, 0x80, 0x30, 0x40,
                                              0x08, 0x10, 0x04, 0x08, 0x82, 0xFC, 0xFF, 0x80,
                                              0x00, 0x44, 0x00, 0x26, 0x01, 0x11, 0x41, 0x08,
                                              0x11, 0x84, 0x04, 0x82, 0xC0, 0x01, 0x40, 0x00};

  uint32_t gothic_18_bold_handle = RESOURCE_ID_GOTHIC_18;
  uint32_t gothic_18_bold_extended_handle = RESOURCE_ID_GOTHIC_18_EXTENDED;
  cl_assert(text_resources_init_font(0, gothic_18_bold_handle, gothic_18_bold_extended_handle, &s_font_info));
  cl_assert(s_font_info.loaded);
  cl_assert(s_font_info.extended);

  uint8_t glyph_size_bytes;
  const GlyphData *glyph;

  glyph = text_resources_get_glyph(&s_font_cache, 'a', &s_font_info);
  cl_assert(glyph != NULL);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(a_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 0x4E50 /* 乐 */, &s_font_info);
  // the chinese pbpack contains the letter 你, it should succeed
  cl_assert(glyph != NULL);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(chinese_glyph_data_bytes, glyph->data, glyph_size_bytes);

  glyph = text_resources_get_glyph(&s_font_cache, 0x8888 /* 袈 */, &s_font_info);
  // the chinese pbpack does not contain the letter 袈, it should return the wildcard
  cl_assert(glyph != NULL);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(chinese_wildcard_bytes, glyph->data, glyph_size_bytes);
}

void test_text_resources__test_emoji_font(void) {
  const uint8_t phone_bytes[] = {0xfe, 0x81, 0x81, 0x3c, 0x66, 0x42, 0xc3, 0xe7, 0xff, 0x00, 0x00, 0x00};

  uint32_t gothic_18_emoji_handle = RESOURCE_ID_GOTHIC_18_EMOJI;
  cl_assert(text_resources_init_font(0, gothic_18_emoji_handle, 0, &s_font_info));

  uint8_t glyph_size_bytes;
  const GlyphData *glyph;

  const Codepoint PHONE_CODEPOINT = 0x260E;
  glyph = text_resources_get_glyph(&s_font_cache, PHONE_CODEPOINT, &s_font_info);
  cl_assert(glyph != NULL);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(phone_bytes, glyph->data, glyph_size_bytes);
}

void DISABLED_test_text_resources__test_emoji_fallback(void) {
  const uint8_t phone_bytes[] = {0xfe, 0x81, 0x81, 0x3c, 0x66, 0x42, 0xc3, 0xe7, 0xff, 0x00, 0x00, 0x00};

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  uint8_t glyph_size_bytes;
  const GlyphData *glyph;

  const Codepoint PHONE_CODEPOINT = 0x260E;
  glyph = text_resources_get_glyph(&s_font_cache, PHONE_CODEPOINT, &s_font_info);
  cl_assert(glyph != NULL);
  glyph_size_bytes = glyph_get_size_bytes(glyph);
  cl_assert_equal_m(phone_bytes, glyph->data, glyph_size_bytes);
}


void test_text_resources__test_glyph_decompression(void) {
  // There is no way to get the list of glyphs present in a font with the existing API. This list
  // of ranges lists the 371 glyphs currently in fontname.ttf
  typedef struct Codepoint_Range {
    uint16_t start;
    uint16_t end;
  } CodePoint_Range;
  const CodePoint_Range codepoint_range[] = {
    { 0x0020, 0x007E }, { 0x00A0, 0x00AC }, { 0x00AE, 0x00D6 }, { 0x00D9, 0x017F },
    { 0x0192, 0x0192 }, { 0x01FC, 0x01FF }, { 0x0218, 0x021B }, { 0x02C6, 0x02DD },
    { 0x03C0, 0x03C0 }, { 0x2013, 0x2014 }, { 0x2018, 0x201A }, { 0x201C, 0x201E },
    { 0x2020, 0x2022 }, { 0x2026, 0x2026 }, { 0x2030, 0x2030 }, { 0x2039, 0x203A },
    { 0x2044, 0x2044 }, { 0x20AC, 0x20AC }, { 0x2122, 0x2122 }, { 0x2126, 0x2126 },
    { 0x2202, 0x2202 }, { 0x2206, 0x2206 }, { 0x220F, 0x220F }, { 0x2211, 0x2212 },
    { 0x221A, 0x221A }, { 0x221E, 0x221E }, { 0x222B, 0x222B }, { 0x2248, 0x2248 },
    { 0x2260, 0x2260 }, { 0x2264, 0x2265 }, { 0x25AF, 0x25AF }, { 0x25CA, 0x25CA },
    { 0xF6C3, 0xF6C3 }, { 0xFB01, 0xFB02 }
  };

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
  uint32_t gothic_18_compressed_handle = RESOURCE_ID_GOTHIC_18_COMPRESSED; // Read source to fix
  cl_assert(text_resources_init_font(0, gothic_18_compressed_handle, 0, &font_info_compressed));
  cl_assert_equal_i(FONT_VERSION(font_info_compressed.base.md.version), 3);
  cl_assert(HAS_FEATURE(font_info_compressed.base.md.version, VERSION_FIELD_FEATURE_RLE4));

  // For each glyph in the font, get both the compressed and uncompressed bit field & header, and
  // assert that they are identical (ignoring any possible garbage after the bitmap).
  // Load the uncompressed glyph into this local glyph_buffer, and use the font cache for the
  // compressed glyph.
  uint8_t glyph_buffer[sizeof(GlyphHeaderData) + CACHE_GLYPH_SIZE];
  for (unsigned index = 0; index < ARRAY_LENGTH(codepoint_range); ++index) {
    for (unsigned codepoint = codepoint_range[index].start;
         codepoint <= codepoint_range[index].end; ++codepoint) {

      const GlyphData *glyph = text_resources_get_glyph(&s_font_cache, codepoint, &s_font_info);
      cl_assert(glyph);

      unsigned glyph_size = sizeof(GlyphHeaderData) + glyph_get_size_bytes(glyph);
      memcpy(glyph_buffer, glyph->data, glyph_size);

      glyph = text_resources_get_glyph(&s_font_cache, codepoint, &font_info_compressed);
      cl_assert(glyph);

      cl_assert_equal_m(glyph->data, glyph_buffer, glyph_size);
    }
  }
}
