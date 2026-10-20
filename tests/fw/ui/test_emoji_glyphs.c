/* SPDX-FileCopyrightText: 2026 Scott Anderson */
/* SPDX-License-Identifier: Apache-2.0 */

// Draws every glyph of every emoji font file into reference images, so a change to any glyph
// shows up in review. Glyphs are grouped into atlases of up to 256 by codepoint block, so a few
// changed glyphs only change their own atlases. A new emoji font gets its atlases automatically.

#include "../graphics/test_graphics.h"
#include "../graphics/util.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/util/size.h>

#include <applib/fonts/fonts_private.h>
#include <applib/graphics/framebuffer.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/text.h>
#include <applib/graphics/text_resources.h>
#include <applib/graphics/utf8.h>
#include <clar.h>
#include <dirent.h>
#include <resource/resource.h>

// Not the system app, so the fonts draw their own glyphs instead of the size-matched emoji font's
#define FONT_APP_NUM 1

#define ATLAS_MAX_GLYPHS 256
#define BLOCK_SHIFT      8
#define CELL_PADDING     2

static FrameBuffer *s_fb;
static GContext s_ctx;
static GBitmap *s_dest_bitmap;

static uint8_t *s_font_data;
static size_t s_font_size;
static uint32_t s_font_resource_id;

// Resource syscalls
//////////////////////

bool sys_resource_is_valid(ResAppNum app_num, uint32_t resource_id) {
  if (app_num == FONT_APP_NUM) {
    return resource_id == s_font_resource_id;
  }
  return resource_is_valid(app_num, resource_id);
}

size_t sys_resource_size(ResAppNum app_num, uint32_t resource_id) {
  if (app_num == FONT_APP_NUM) {
    return (resource_id == s_font_resource_id) ? s_font_size : 0;
  }
  return resource_size(app_num, resource_id);
}

size_t sys_resource_load_range(ResAppNum app_num, uint32_t resource_id, uint32_t offset,
                               uint8_t *buffer, size_t num_bytes) {
  if (app_num != FONT_APP_NUM) {
    return resource_load_byte_range_system(app_num, resource_id, offset, buffer, num_bytes);
  }
  if (resource_id != s_font_resource_id || offset >= s_font_size) {
    return 0;
  }
  const size_t count = MIN(num_bytes, s_font_size - offset);
  memcpy(buffer, s_font_data + offset, count);
  return count;
}

// Setup and Teardown
//////////////////////

void test_emoji_glyphs__initialize(void) {
  s_fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(s_fb, &(GSize){DISP_COLS, DISP_ROWS});
  test_graphics_context_init(&s_ctx, s_fb);
  framebuffer_clear(s_fb);
}

void test_emoji_glyphs__cleanup(void) {
  free(s_fb);
  s_fb = nullptr;
  gbitmap_destroy(s_dest_bitmap);
  s_dest_bitmap = nullptr;
  free(s_font_data);
  s_font_data = nullptr;
}

// Helpers
//////////////////////

static void prv_load_font_file(const char *path) {
  FILE *file = fopen(path, "rb");
  cl_assert_(file, path);
  cl_assert_equal_i(fseek(file, 0, SEEK_END), 0);
  s_font_size = ftell(file);
  cl_assert_equal_i(fseek(file, 0, SEEK_SET), 0);
  free(s_font_data);
  s_font_data = malloc(s_font_size);
  cl_assert_equal_i(fread(s_font_data, 1, s_font_size, file), s_font_size);
  fclose(file);
  // A new id per font keeps glyphs of the previous font out of the cache
  s_font_resource_id++;
}

static int prv_compare_codepoints(const void *a, const void *b) {
  const Codepoint ca = *(const Codepoint *)a;
  const Codepoint cb = *(const Codepoint *)b;
  return (ca > cb) - (ca < cb);
}

// Reads the codepoints from the font's offset tables, sorted
static Codepoint *prv_font_codepoints(size_t *count_out) {
  FontMetaDataV3 md;
  cl_assert(s_font_size >= sizeof(md));
  memcpy(&md, s_font_data, sizeof(md));
  cl_assert_equal_i(md.version, 3);

  const size_t offset_bytes = (md.features & FEATURE_OFFSET_16) ? 2 : 4;
  const size_t entry_size = md.codepoint_bytes + offset_bytes;
  const uint8_t *hash_table = s_font_data + md.size;
  const uint8_t *offset_tables = hash_table + md.hash_table_size * sizeof(FontHashTableEntry);

  Codepoint *codepoints = malloc(md.number_of_glyphs * sizeof(Codepoint));
  size_t count = 0;
  for (int i = 0; i < md.hash_table_size; i++) {
    FontHashTableEntry entry;
    memcpy(&entry, hash_table + i * sizeof(entry), sizeof(entry));
    for (int j = 0; j < entry.count; j++) {
      Codepoint codepoint = 0;
      memcpy(&codepoint, offset_tables + entry.offset + j * entry_size, md.codepoint_bytes);
      cl_assert(count < md.number_of_glyphs);
      codepoints[count++] = codepoint;
    }
  }
  cl_assert_equal_i(count, md.number_of_glyphs);
  qsort(codepoints, count, sizeof(Codepoint), prv_compare_codepoints);
  *count_out = count;
  return codepoints;
}

// The box all the glyphs draw in, relative to the start of their line
static GRect prv_glyphs_bounds(FontInfo *font, const Codepoint *codepoints, size_t count) {
  int16_t min_x = 0, min_y = 0, max_x = 1, max_y = font->max_height;
  for (size_t i = 0; i < count; i++) {
    const GRect frame = text_resources_get_glyph_frame(&s_ctx.font_cache, codepoints[i], font);
    if (frame.size.w <= 0 || frame.size.h <= 0) {
      continue;
    }
    min_x = MIN(min_x, frame.origin.x);
    min_y = MIN(min_y, frame.origin.y);
    max_x = MAX(max_x, frame.origin.x + frame.size.w);
    max_y = MAX(max_y, frame.origin.y + frame.size.h);
  }
  return GRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

static void prv_prepare_canvas(GSize size) {
  gbitmap_destroy(s_dest_bitmap);
  s_dest_bitmap = gbitmap_create_blank(size, GBitmapFormat8Bit);
  memset(s_dest_bitmap->addr, GColorWhite.argb,
         s_dest_bitmap->row_size_bytes * s_dest_bitmap->bounds.size.h);
  s_ctx.dest_bitmap = *s_dest_bitmap;
  s_ctx.draw_state.clip_box.size = size;
  s_ctx.draw_state.drawing_box.size = size;
}

// Draws the glyphs in a grid about as wide as it is tall and compares it with its reference image
static bool prv_check_atlas(FontInfo *font, const Codepoint *codepoints, size_t count,
                            const char *name) {
  const GRect bounds = prv_glyphs_bounds(font, codepoints, count);
  int columns = 1;
  while (columns * columns < (int)count) {
    columns++;
  }
  const int rows = (count + columns - 1) / columns;
  const GSize cell = {bounds.size.w + 2 * CELL_PADDING, bounds.size.h + 2 * CELL_PADDING};
  prv_prepare_canvas(GSize(columns * cell.w, rows * cell.h));
  graphics_context_set_text_color(&s_ctx, GColorBlack);

  for (size_t i = 0; i < count; i++) {
    utf8_t text[5] = {};
    utf8_encode_codepoint(codepoints[i], text);
    const GRect box = {
      .origin =
          {(i % columns) * cell.w + CELL_PADDING - bounds.origin.x,
           (i / columns) * cell.h + CELL_PADDING - bounds.origin.y},
      .size = {bounds.origin.x + bounds.size.w + cell.w, 2 * cell.h},
    };
    graphics_draw_text(&s_ctx, (const char *)text, font, box, GTextOverflowModeFill,
                       GTextAlignmentLeft, nullptr);
  }

  char filename[PATH_STRING_LENGTH];
  snprintf(filename, sizeof(filename), "test_emoji_glyphs__%s.pbi", name);
  return gbitmap_pbi_eq(s_dest_bitmap, filename);
}

// EMOJI_28~color.pbf -> emoji_28_color
static void prv_font_image_name(const char *file_name, char *out, size_t out_size) {
  snprintf(out, out_size, "%.*s", (int)(strlen(file_name) - strlen(".pbf")), file_name);
  for (char *c = out; *c; c++) {
    *c = (*c == '~') ? '_' : tolower((unsigned char)*c);
  }
}

// Atlases take whole 256-codepoint blocks, merging neighbors while they fit, so adding a glyph
// only moves the glyphs of its own atlas
static int prv_check_font(const char *file_name) {
  char path[PATH_STRING_LENGTH];
  snprintf(path, sizeof(path), "%s/%s", FIRMWARE_EMOJI_DIR, file_name);
  prv_load_font_file(path);

  FontInfo font = {};
  cl_assert_(text_resources_init_font(FONT_APP_NUM, s_font_resource_id, 0, &font), path);
  text_resources_reset_cache(&s_ctx.font_cache);

  size_t count;
  Codepoint *codepoints = prv_font_codepoints(&count);

  char font_name[64];
  prv_font_image_name(file_name, font_name, sizeof(font_name));

  int failures = 0;
  size_t start = 0;
  while (start < count) {
    size_t end = start;
    while (end < count) {
      // Take the whole block of codepoints[end]
      size_t block_end = end;
      while (block_end < count &&
             (codepoints[block_end] >> BLOCK_SHIFT) == (codepoints[end] >> BLOCK_SHIFT)) {
        block_end++;
      }
      if (end > start && block_end - start > ATLAS_MAX_GLYPHS) {
        break;
      }
      end = block_end;
    }

    char name[96];
    snprintf(name, sizeof(name), "%s_%04" PRIx32, font_name,
             (codepoints[start] >> BLOCK_SHIFT) << BLOCK_SHIFT);
    if (!prv_check_atlas(&font, &codepoints[start], end - start, name)) {
      printf("Atlas %s differs\n", name);
      failures++;
    }
    start = end;
  }

  free(codepoints);
  return failures;
}

static int prv_compare_names(const void *a, const void *b) {
  return strcmp(*(char *const *)a, *(char *const *)b);
}

// Tests
//////////////////////

void test_emoji_glyphs__atlases(void) {
  DIR *dir = opendir(FIRMWARE_EMOJI_DIR);
  cl_assert_(dir, FIRMWARE_EMOJI_DIR);
  char *names[64];
  size_t num_names = 0;
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    const size_t len = strlen(entry->d_name);
    if (strncmp(entry->d_name, "EMOJI_", strlen("EMOJI_")) == 0 && len > strlen(".pbf") &&
        strcmp(entry->d_name + len - strlen(".pbf"), ".pbf") == 0) {
      cl_assert(num_names < ARRAY_LENGTH(names));
      names[num_names++] = strdup(entry->d_name);
    }
  }
  closedir(dir);
  cl_assert(num_names > 0);
  qsort(names, num_names, sizeof(names[0]), prv_compare_names);

  int failures = 0;
  for (size_t i = 0; i < num_names; i++) {
    failures += prv_check_font(names[i]);
    free(names[i]);
  }
  cl_assert_equal_i(failures, 0);
}
