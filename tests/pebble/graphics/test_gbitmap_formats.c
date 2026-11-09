/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <applib/graphics/gtypes.h>
#include <clar.h>

// stubs

/////////////////////////////

void test_gbitmap_formats__create_blank(void) {
  GBitmap *bmp = nullptr;

  cl_assert((void *)&bmp->palette == (void *)&bmp->data_row_infos); // union with .palette

#ifdef CONFIG_PLATFORM_GABBRO
  const GSize s10 = GSize(10, 10);
  const GSize s_full = GSize(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT);

  bmp = gbitmap_create_blank(s10, GBitmapFormat1Bit);
  cl_assert(nullptr != bmp);
  cl_assert(nullptr == bmp->data_row_infos);

  bmp = gbitmap_create_blank(s10, GBitmapFormat8Bit);
  cl_assert(nullptr != bmp);
  cl_assert(nullptr == bmp->data_row_infos);

  bmp = gbitmap_create_blank(s10, GBitmapFormat1BitPalette);
  cl_assert(nullptr != bmp);
  cl_assert(nullptr != bmp->palette);

  bmp = gbitmap_create_blank(s10, GBitmapFormat2BitPalette);
  cl_assert(nullptr != bmp);
  cl_assert(nullptr != bmp->palette);

  bmp = gbitmap_create_blank(s10, GBitmapFormat4BitPalette);
  cl_assert(nullptr != bmp);
  cl_assert(nullptr != bmp->palette);

  bmp = gbitmap_create_blank(s10, GBitmapFormat8BitCircular);
  cl_assert(nullptr == bmp);

  bmp = gbitmap_create_blank(s_full, GBitmapFormat8BitCircular);
  cl_assert(nullptr != bmp);
  cl_assert(nullptr == bmp->data_row_infos);
  GBitmapDataRowInfo info = gbitmap_get_data_row_info(bmp, 0);
  cl_assert_equal_p(bmp->addr, info.data);
  cl_assert_equal_i(113, info.min_x);
  cl_assert_equal_i(146, info.max_x);
  info = gbitmap_get_data_row_info(bmp, PBL_DISPLAY_HEIGHT / 2);
  cl_assert_equal_p((uint8_t *)bmp->addr + PBL_DISPLAY_HEIGHT / 2 * PBL_DISPLAY_WIDTH, info.data);
  cl_assert_equal_i(0, info.min_x);
  cl_assert_equal_i(PBL_DISPLAY_WIDTH - 1, info.max_x);
  info = gbitmap_get_data_row_info(bmp, PBL_DISPLAY_HEIGHT - 1);
  cl_assert_equal_i(113, info.min_x);
  cl_assert_equal_i(146, info.max_x);
#endif
}

void test_gbitmap_formats__create_blank_with_palette(void) {
#ifdef CONFIG_PLATFORM_GABBRO
  const GSize s10 = GSize(10, 10);
  const GSize s_full = GSize(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT);
  GBitmap *bmp;
  GColor8 *p = (GColor8 *)&p; // some value to test against

  cl_assert(nullptr == gbitmap_create_blank_with_palette(s10, GBitmapFormat1Bit, p, true));
  cl_assert(nullptr == gbitmap_create_blank_with_palette(s10, GBitmapFormat8Bit, p, true));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat1BitPalette, p, true);
  cl_assert(nullptr != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat2BitPalette, p, true);
  cl_assert(nullptr != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat4BitPalette, p, true);
  cl_assert(nullptr != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  cl_assert(nullptr == gbitmap_create_blank_with_palette(s10, GBitmapFormat8BitCircular, p, true));
  cl_assert(nullptr ==
            gbitmap_create_blank_with_palette(s_full, GBitmapFormat8BitCircular, p, true));
#endif
}

void test_gbitmap_formats__display_framebuffer_bytes(void) {
#ifdef CONFIG_PLATFORM_FLINT
  const size_t expected = 20 * 168; // 20 * 8 == 144px + 2 bytes padding per scanline
#endif
#ifdef CONFIG_PLATFORM_EMERY
  const size_t expected = 200 * 228;
#endif
#ifdef CONFIG_PLATFORM_GABBRO
  const size_t expected = 260 * 260;
#endif
  cl_assert_equal_i(expected, DISPLAY_FRAMEBUFFER_BYTES);
}

size_t prv_gbitmap_size_for_data(GSize size, GBitmapFormat format);

void test_gbitmap_formats__size_for_data(void) {
  cl_assert_equal_i(40, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat1Bit));
  cl_assert_equal_i(130, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat8Bit));
  cl_assert_equal_i(20, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat1BitPalette));
  cl_assert_equal_i(40, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat2BitPalette));
  cl_assert_equal_i(70, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat4BitPalette));
  cl_assert_equal_i(0, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat8BitCircular));

  const size_t expected = PBL_IF_RECT_ELSE(0, DISPLAY_FRAMEBUFFER_BYTES);
  cl_assert_equal_i(expected,
                    prv_gbitmap_size_for_data(GSize(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT),
                                              GBitmapFormat8BitCircular));
}
