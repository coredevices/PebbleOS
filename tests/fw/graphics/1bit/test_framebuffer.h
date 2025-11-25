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

typedef struct stat STAT_T;

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define IMAGE_DWORDS_PER_LINE 6

static void print_framebuffer_as_literal(const char* unit_name, const FrameBuffer* framebuffer, int col, int row, int cols, int rows) {
  printf("\n-- %s image --\n", unit_name);
  printf("  uint32_t image[] = {\n");
  int num_words = 0;
  for (int j = row; j < row+rows && j < FrameBuffer_MaxY; j++) {
    bool j_more = j+1 < row+rows && j+1 < FrameBuffer_MaxY;
    for (int i = col; i < col+cols && i < FRAMEBUFFER_WORDS_PER_ROW; i++) {
      bool i_more = i+1 < col+cols && i+1 < FRAMEBUFFER_WORDS_PER_ROW;
      num_words++;
      if (num_words % IMAGE_DWORDS_PER_LINE == 1) { printf("    "); }
      printf("0x%x", framebuffer->buffer[j*FRAMEBUFFER_WORDS_PER_ROW+i]);
      if (i_more || j_more) {
        printf(",");
        if (num_words % IMAGE_DWORDS_PER_LINE == 0) { printf("\n"); }
      }
    }
  }
  printf("\n  };\n");
}

static void fread_pbi_header(FILE* file, GBitmap* bitmap) {
  fread(&bitmap->row_size_bytes, sizeof(bitmap->row_size_bytes), 1, file);
  fread(&bitmap->info_flags, sizeof(bitmap->info_flags), 1, file);
  fread(&bitmap->bounds, sizeof(bitmap->bounds), 1, file);
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
  uint16_t row_size_bytes = (FrameBuffer_MaxX + 31) / 32 * 4;
  uint16_t info_flags = 1 << 1;
  GRect bounds = GRect(0, 0, FrameBuffer_MaxX, FrameBuffer_MaxY);
  fwrite(&row_size_bytes, sizeof(row_size_bytes), 1, file);
  fwrite(&info_flags, sizeof(info_flags), 1, file);
  fwrite(&bounds, sizeof(bounds), 1, file);
  fwrite(framebuffer->buffer, sizeof(framebuffer->buffer[0]), row_size_bytes/4*FrameBuffer_MaxY, file);
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

static bool framebuffer_eq_image_raw(const FrameBuffer* framebuffer, uint32_t* image, int col, int row, int cols, int rows) {
  for (int j = row; j < row+rows && j < FrameBuffer_MaxY; j++) {
    for (int i = col; i < col+cols && i < FRAMEBUFFER_WORDS_PER_ROW; i++) {
      int fb_index = j*FRAMEBUFFER_WORDS_PER_ROW+i;
      int img_index = (j-row)*cols+(i-col);
      uint32_t number_of_bits = col < IMAGE_DWORDS_PER_LINE-1 ? 32 : DISP_COLS % 32;
      uint32_t mask = number_of_bits == 32 ? -1 : ((1 << number_of_bits) - 1);
      uint32_t fb_part = mask & framebuffer->buffer[fb_index];
      uint32_t img_part = mask & image[img_index];
      if (fb_part != img_part) {
        printf("\nframebuffer[%d] != image[%d], (0x%x, 0x%x) col=%d row=%d\n",
          fb_index, img_index, framebuffer->buffer[fb_index], image[img_index], i, j);
        return false;
      }
    }
  }
  return true;
}

static bool framebuffer_eq_image(const char* unit_name, const FrameBuffer* framebuffer, uint32_t* image, int col, int row, int cols, int rows) {
#ifndef TEST_GRAPHICS_SILENT
  print_framebuffer_as_literal(unit_name, framebuffer, col, row, cols, rows);
#endif
  return framebuffer_eq_image_raw(framebuffer, image, col, row, cols, rows);
}

static bool framebuffer_eq(const char* unit_name, const FrameBuffer* framebuffer, FrameBuffer* other) {
  return framebuffer_eq_image(unit_name, framebuffer, other->buffer, 0, 0, FRAMEBUFFER_WORDS_PER_ROW, FrameBuffer_MaxY);
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
  if (!framebuffer_eq_image_raw(framebuffer, buffer, 0, 0, bitmap.row_size_bytes/4, bitmap.bounds.size.h)) {
    printf("\ndoes not match screenshot %s\n", filename);
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
    for (int i = 0; i < FRAMEBUFFER_WORDS_PER_ROW; i++) {
      int fb_index = j*FRAMEBUFFER_WORDS_PER_ROW+i;
      if (framebuffer->buffer[fb_index] != (gcolor_equal(color, GColorBlack) ? 0 : 0xffffffff)) {
        printf("\nframebuffer[%d] is not empty(%d), has 0x%x, col=%d row=%d\n",
          fb_index, color.argb, framebuffer->buffer[fb_index], i, j);
        return false;
      }
    }
  }
  return true;
}
