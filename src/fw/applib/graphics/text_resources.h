/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/fonts/fonts_private.h"
#include "applib/fonts/codepoint.h"
#include "util/keyed_circular_cache.h"

#include <stdint.h>

typedef struct __attribute__((__packed__)) {
  uint8_t width_px;
  union {
    uint8_t height_px;
    uint8_t num_rle_units;
  };
  int8_t left_offset_px;
  int8_t top_offset_px;
  int8_t horiz_advance;
} GlyphHeaderData;

typedef struct __attribute__((__packed__)) {
  uint8_t width_px;
  uint8_t height_px;
  int8_t left_offset_px;
  int8_t top_offset_px;
  uint8_t empty[3];
  int8_t horiz_advance;
} GlyphHeaderDataV1;

typedef struct __attribute__((__packed__)) {
  GlyphHeaderData header;
  uint32_t data[];
} GlyphData;

//! Maps a codepoint to the location of the actual font data.
typedef struct __attribute__((__packed__)) {
  Codepoint codepoint : 16;
  uint16_t offset;
} OffsetTableEntry_2_2;

typedef struct __attribute__((__packed__)) {
  Codepoint codepoint : 16;
  uint32_t offset;
} OffsetTableEntry_2_4;

typedef struct __attribute__((__packed__)) {
  Codepoint codepoint;
  uint32_t offset;
} OffsetTableEntry_4_4;

typedef struct __attribute__((__packed__)) {
  Codepoint codepoint;
  uint16_t offset;
} OffsetTableEntry_4_2;

#if !defined(MAX_FONT_GLYPH_SIZE)
  #define MAX_FONT_GLYPH_SIZE 256
#endif

// Slightly bigger than the biggest glyph we have
// This is the size in bytes for the glyph bitmap data.
#define CACHE_GLYPH_SIZE MAX_FONT_GLYPH_SIZE

typedef struct {
  uint32_t resource_offset;
  //! Whether the bitmap data in this structure is valid.
  bool is_bitmap_loaded;

  union {
#if CAPABILITY_HAS_GLYPH_BITMAP_CACHING
    //! Glyph data including bitmap
    struct __attribute__((__packed__)) {
      GlyphHeaderData header_data;
      uint8_t data[CACHE_GLYPH_SIZE];
    };
#else
    //! Glyph data without a bitmap
    GlyphHeaderData header_data;
#endif
    GlyphData glyph_data;
  };
} LineCacheData;

#define LINE_CACHE_SIZE 30

// Allow 1K max for offset tables
#define OFFSET_TABLE_MAX_SIZE (1024)

typedef struct FontCache {
  int offset_table_id;
  uint16_t offset_table_size;
  //! The currently loaded font's offset table.
  //! @note this needs to be able to accomodate legacy fonts
  union {
    OffsetTableEntry_2_2 offsets_buffer_2_2[OFFSET_TABLE_MAX_SIZE / sizeof(OffsetTableEntry_2_2)];
    OffsetTableEntry_2_4 offsets_buffer_2_4[OFFSET_TABLE_MAX_SIZE / sizeof(OffsetTableEntry_2_4)];
    OffsetTableEntry_4_2 offsets_buffer_4_2[OFFSET_TABLE_MAX_SIZE / sizeof(OffsetTableEntry_4_2)];
    OffsetTableEntry_4_4 offsets_buffer_4_4[OFFSET_TABLE_MAX_SIZE / sizeof(OffsetTableEntry_4_4)];
  };
  //! line_cache's backing storage for keys
  KeyedCircularCacheKey cache_keys[LINE_CACHE_SIZE];
  //! line_cache's backing storage for data
  LineCacheData cache_data[LINE_CACHE_SIZE];
  //! some scratch space so we don't need to create a LineCacheData on the stack
  LineCacheData cache_data_scratch;

  // Since we don't have bitmap caching, we need to have somewhere to store the bitmap data.
#if !CAPABILITY_HAS_GLYPH_BITMAP_CACHING
  //! cache_key for the last used glyph
  uint32_t glyph_buffer_key;
  //! data for the last used glyph
  uint8_t glyph_buffer[sizeof(LineCacheData) + CACHE_GLYPH_SIZE];
#endif
  KeyedCircularCache line_cache;
  const FontResource *cached_font;
} FontCache;

const GlyphData *text_resources_get_glyph(FontCache *font_cache, Codepoint codepoint,
                                          FontInfo *font_info);

int8_t text_resources_get_glyph_horiz_advance(FontCache *font_cache, Codepoint codepoint,
                                              FontInfo *font_info);

//! Initialize a FontInfo struct with resource contents
//! A FontInfo contains references to up to *two* font resources: a "base" font and an "extension".
//! The base font is part of the system resources pack and contains latin characters and emoji
//! The extension font contains additional characters required to display localized UI or
//! notifications.
//! @note the extension may not be installed and may be removed at any give time
//! @param app_num the ResAppNum associated with this font (i.e. system or app?)
//! @param font_resource the "base" resource id
//! @param extension_resource the "extension" resource id
//! @fontinfo a pointer to the fontinfo struct to initialize
bool text_resources_init_font(ResAppNum app_num, uint32_t font_resource,
                              uint32_t extension_resource, FontInfo *font_info);

