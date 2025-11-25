/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/graphics/framebuffer.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "../test_graphics.h"
#include "../util.h"

typedef struct stat STAT_T;

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define IMAGE_DWORDS_PER_LINE 6

static void print_framebuffer_as_literal(const char* unit_name, const FrameBuffer* framebuffer,
                                         int col, int row, int cols, int rows) {
  printf("\n-- %s image --\n", unit_name);
  printf("  uint8_t image[] = {\n");
  for (int y = row; y < row+rows && y < FrameBuffer_MaxY; ++y) {
    for (int x = col; x < col + cols && x < FrameBuffer_MaxX; ++x) {
      printf("0x%x", framebuffer->buffer[y*FRAMEBUFFER_BYTES_PER_ROW + x]);
    }
    printf("\n");
  }
  printf("\n  };\n");
}

static void fread_pbi_header(FILE* file, GBitmap* bitmap) {
  fread(&bitmap->row_size_bytes, sizeof(bitmap->row_size_bytes), 1, file);
  fread(&bitmap->info_flags, sizeof(bitmap->info_flags), 1, file);
  fread(&bitmap->bounds, sizeof(bitmap->bounds), 1, file);
  bitmap->info.format = GBitmapFormat1Bit;
}

static uint32_t* fread_pbi(FILE* file, GBitmap* bitmap) {
  fread_pbi_header(file, bitmap);
  size_t image_size = bitmap->row_size_bytes/4 * bitmap->bounds.size.h;
  uint32_t* buffer = bitmap->addr = (uint32_t*) malloc(image_size*4);
  if (buffer) {
    fread(buffer, 4, image_size, file);
    return buffer;
  }
  return NULL;
}

static uint32_t* read_pbi(const char* filename, GBitmap* bitmap) {
  char res_path[strlen(CLAR_FIXTURE_PATH) + 1 + strlen(GRAPHICS_FIXTURE_PATH) + 1 + strlen(filename) + 1];
  sprintf(res_path, "%s/%s/%s", CLAR_FIXTURE_PATH, GRAPHICS_FIXTURE_PATH, filename);
  FILE* file = fopen(res_path, "r");
  if (!file) {
    printf("\ncould not open %s for reading\n", res_path);
    return NULL;
  }
  uint32_t* buffer = fread_pbi(file, bitmap);
  fclose(file);
  return buffer;
}

static void free_pbi(GBitmap* bitmap) {
  if (bitmap->addr) {
    free(bitmap->addr);
  }
}

static void fwrite_screenshot_from_framebuffer(FILE* file, const FrameBuffer* framebuffer) {
  uint16_t row_size_bytes = FRAMEBUFFER_BYTES_PER_ROW;
  uint16_t info_flags = 1 << 1;
  GRect bounds = GRect(0, 0, FrameBuffer_MaxX, FrameBuffer_MaxY);
  fwrite(&row_size_bytes, sizeof(row_size_bytes), 1, file);
  fwrite(&info_flags, sizeof(info_flags), 1, file);
  fwrite(&bounds, sizeof(bounds), 1, file);
  fwrite(framebuffer->buffer, sizeof(framebuffer->buffer[0]), FRAMEBUFFER_SIZE_BYTES, file);
}

static bool write_screenshot_from_framebuffer(const char* filename, const FrameBuffer* framebuffer) {
  FILE* file = fopen(filename, "w");
  if (!file) {
    printf("\ncould not open %s for writing a screenshot\n", filename);
    return false;
  }
  fwrite_screenshot_from_framebuffer(file, framebuffer);
  fclose(file);
  return true;
}

static bool framebuffer_eq_image_raw(const FrameBuffer* framebuffer, uint8_t* image, int col,
                                     int row, int cols, int rows) {
  const uint8_t *fb = framebuffer->buffer;
  for (int y = row; y < row+rows && y < FrameBuffer_MaxY; ++y) {
    for (int x = col; x < col+cols && x < FrameBuffer_MaxX; ++x) {
      int fb_index = y * FRAMEBUFFER_BYTES_PER_ROW + x;
      int img_index = (y-row)*FRAMEBUFFER_BYTES_PER_ROW + (x-col);
      if (fb[fb_index] != image[img_index]) {
        printf("\nframebuffer[%d] != image[%d], (0x%x, 0x%x) col=%d row=%d\n",
          fb_index, img_index, fb[fb_index], image[img_index], x, y);
        return false;
      }
    }
  }
  return true;
}

static bool framebuffer_eq_image(const char* unit_name, const FrameBuffer* framebuffer,
                                 uint8_t* image, int col, int row, int cols, int rows) {
#ifndef TEST_GRAPHICS_SILENT
  print_framebuffer_as_literal(unit_name, framebuffer, col, row, cols, rows);
#endif
  return framebuffer_eq_image_raw(framebuffer, image, col, row, cols, rows);
}

static bool framebuffer_eq(const char* unit_name, const FrameBuffer* framebuffer,
                           FrameBuffer* other) {
  return framebuffer_eq_image(unit_name, framebuffer, other->buffer, 0, 0,
                              FRAMEBUFFER_BYTES_PER_ROW, FrameBuffer_MaxY);
}

static void convert_to_8bit_image(const uint32_t *in, GBitmap *bmp, uint8_t *out) {
  for (int y = 0; y < bmp->bounds.size.h; ++y) {
    for (int x = 0; x < bmp->bounds.size.w; ++x) {
      int in_idx = y * (bmp->row_size_bytes/4) + (x / 32);
      int bit_idx = x % 32;
      bool set = in[in_idx] & (1 << bit_idx);

      int out_idx = y * bmp->bounds.size.w + x;
      out[out_idx] = (set) ? GColorWhite.argb : GColorBlack.argb;
    }
  }
}

static bool framebuffer_eq_screenshot_raw(const FrameBuffer* framebuffer, const char* filename) {
  GBitmap bitmap;
  FILE* file = fopen(filename, "r");
  if (!file) {
    printf("\nfailed to open %s\n", filename);
    return false;
  }
  fread_pbi_header(file, &bitmap);
  uint32_t buffer[bitmap.row_size_bytes/4 * bitmap.bounds.size.h];
  fread(buffer, 1, sizeof(buffer), file);
  fclose(file);

  uint8_t image[bitmap.bounds.size.w * bitmap.bounds.size.h];
  convert_to_8bit_image(buffer, &bitmap, image);
  if (!framebuffer_eq_image_raw(framebuffer, image, 0, 0, bitmap.row_size_bytes,
                                bitmap.bounds.size.h)) {
    bitmap.info.format = GBitmapFormat8Bit;
    bitmap.row_size_bytes = FRAMEBUFFER_BYTES_PER_ROW;
    printf("\ndoes not match screenshot %s\n", filename);
    printf("Good:\n");
    bitmap.addr = &image;
    print_bitmap(&bitmap);
    printf("Bad:\n");
    bitmap.addr = (void*)&framebuffer->buffer;
    print_bitmap(&bitmap);
    return false;
  }
  return true;
}

static bool framebuffer_eq_screenshot(const FrameBuffer* framebuffer, const char* filename) {
  char ref_path[strlen(CLAR_FIXTURE_PATH) + 1 + strlen(GRAPHICS_FIXTURE_PATH) + 1 + strlen(filename) + 1];
  sprintf(ref_path, "%s/%s/%s", CLAR_FIXTURE_PATH, GRAPHICS_FIXTURE_PATH, filename);
  STAT_T st;
  if (stat(ref_path, &st) != 0 || !framebuffer_eq_screenshot_raw(framebuffer, ref_path)) {
    char out_path[strlen(GRAPHICS_FIXTURE_OUT_PATH) + 1 + strlen(filename) + 1];
    sprintf(out_path, "%s/%s", GRAPHICS_FIXTURE_OUT_PATH,filename);
    write_screenshot_from_framebuffer(out_path, framebuffer);
    char cwd[PATH_MAX];
    if (!getcwd(cwd, PATH_MAX)) {
      printf("\nfailed to get working directory\n");
      return false;
    }
    printf("\ngenerated %s/%s\n", cwd, out_path);
    return false;
  }
  return true;
}

static bool framebuffer_is_empty(const char* unit_name, FrameBuffer* framebuffer, GColor color) {
  for (int j = 0; j < FrameBuffer_MaxY; j++) {
    for (int i = 0; i < FRAMEBUFFER_BYTES_PER_ROW; i++) {
      int fb_index = j*FRAMEBUFFER_BYTES_PER_ROW + i;
      if (framebuffer->buffer[fb_index] != color.argb) {
        printf("\nframebuffer[%d] is not empty(%d), has 0x%x, col=%d row=%d\n",
          fb_index, color.argb, framebuffer->buffer[fb_index], i, j);
        return false;
      }
    }
  }
  return true;
}
