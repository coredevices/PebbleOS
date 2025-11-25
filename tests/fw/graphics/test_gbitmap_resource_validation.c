/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gtypes.h"
#include "applib/graphics/gbitmap_pbi.h"

#include "stubs_app_state.h"
#include "stubs_graphics_context.h"
#include "stubs_logging.h"
#include "stubs_process_manager.h"
#include "stubs_passert.h"

// Stubs
///////////////////////
bool gbitmap_init_with_png_data(GBitmap *bitmap, const uint8_t *data, size_t data_size) {
  return false;
}

bool gbitmap_png_data_is_png(const uint8_t *data, size_t data_size) {
  return false;
}

ResAppNum sys_get_current_resource_num(void) {
  return 0;
}

const uint8_t *sys_resource_read_only_bytes(ResAppNum app_num, uint32_t resource_id,
                                            size_t *num_bytes_out) {
  return NULL;
}

// Fakes
///////////////////////
size_t s_resource_size;
size_t sys_resource_size(ResAppNum app_num, uint32_t resource_id) {
  return s_resource_size;
}

typedef struct {
  uint16_t row_size_bytes;
  union {
    uint16_t info_flags;
    BitmapInfo info;
  };
  uint16_t width;
  uint16_t height;
} FakeBitmapData;

FakeBitmapData s_fake_bitmap_data;

size_t sys_resource_load_range(
    ResAppNum app_num, uint32_t id, uint32_t start_offset, uint8_t *data, size_t num_bytes) {

  BitmapData *bitmap = (BitmapData*) data;
  *bitmap = (BitmapData) {
    .row_size_bytes = s_fake_bitmap_data.row_size_bytes,
    .info_flags = s_fake_bitmap_data.info_flags,
    .width = s_fake_bitmap_data.width,
    .height = s_fake_bitmap_data.height
  };

  return num_bytes;
}


// Tests
///////////////////////

void test_gbitmap_resource_validation__initialize(void) {
  s_resource_size = 0;
  s_fake_bitmap_data = (FakeBitmapData) { 0 };
}

static uint32_t prv_calculate_size(FakeBitmapData *bitmap) {
  const uint32_t required_size_bytes =
      offsetof(BitmapData, data) + // header size
      (bitmap->row_size_bytes * bitmap->height) + // pixel data
      gbitmap_get_palette_size(bitmap->info.format); // palette data
  return required_size_bytes;
}

void test_gbitmap_resource_validation__total_size(void) {
  s_fake_bitmap_data = (FakeBitmapData) {
    .row_size_bytes = 8,
    .info.format = GBitmapFormat8Bit,
    .info.version = GBITMAP_VERSION_1,
    .width = 8,
    .height = 1
  };

  // Set the resource size to be valid.
  s_resource_size = prv_calculate_size(&s_fake_bitmap_data);

  // We should load it successfully
  GBitmap bitmap;
  cl_assert(gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // However, if we corrupt the row_size_bytes field we should fail.
  s_fake_bitmap_data.row_size_bytes = 12;
  cl_assert(!gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // Corrupt it the other way, so that there's not enough data
  s_fake_bitmap_data.row_size_bytes = 4;
  cl_assert(!gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // Fix it up again
  s_fake_bitmap_data.row_size_bytes = 8;
  cl_assert(gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // But now change the palette format to something that requires more space and watch it fail
  s_fake_bitmap_data.info.format = GBitmapFormat4BitPalette;
  cl_assert(!gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // But if we have space for the palette, it should pass
  s_resource_size = prv_calculate_size(&s_fake_bitmap_data);
  cl_assert(gbitmap_init_with_resource_system(&bitmap, 0, 0));
}

void test_gbitmap_resource_validation__row_size(void) {
  s_fake_bitmap_data = (FakeBitmapData) {
    .row_size_bytes = 8,
    .info.format = GBitmapFormat8Bit,
    .info.version = GBITMAP_VERSION_1,
    .width = 8,
    .height = 1
  };

  // Set the resource size to be valid.
  s_resource_size = prv_calculate_size(&s_fake_bitmap_data);

  // We should load it successfully
  GBitmap bitmap;
  cl_assert(gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // Vary the width without changing the height to be too large
  s_fake_bitmap_data.width = 10,
  cl_assert(!gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // Too small is fine though
  s_fake_bitmap_data.width = 6,
  cl_assert(gbitmap_init_with_resource_system(&bitmap, 0, 0));

  // Test with an uneven number of bits and make sure we're rounding correctly
  s_fake_bitmap_data.info.format = GBitmapFormat1Bit;
  s_fake_bitmap_data.width = 64;
  cl_assert(gbitmap_init_with_resource_system(&bitmap, 0, 0));

  s_fake_bitmap_data.info.format = GBitmapFormat1Bit;
  s_fake_bitmap_data.width = 65;
  cl_assert(!gbitmap_init_with_resource_system(&bitmap, 0, 0));

  s_fake_bitmap_data.info.format = GBitmapFormat1Bit;
  s_fake_bitmap_data.width = 63;
  cl_assert(gbitmap_init_with_resource_system(&bitmap, 0, 0));
}
