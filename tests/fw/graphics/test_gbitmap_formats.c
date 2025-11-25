/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/gtypes.h"

#include "clar.h"

#include <stdio.h>

// stubs
#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_process_manager.h"

/////////////////////////////

const void * const g_gbitmap_spalding_data_row_infos = &g_gbitmap_spalding_data_row_infos;

void test_gbitmap_formats__create_blank(void) {
  const GSize s10 = GSize(10, 10);
  const GSize s180 = GSize(180, 180);
  GBitmap *bmp = NULL;

  cl_assert((void*)&bmp->palette == (void*)&bmp->data_row_infos); // union with .palette
  cl_assert(NULL != g_gbitmap_spalding_data_row_infos); // make sure unit-test fixture is ok


#ifdef PLATFORM_TINTIN
  cl_assert(!process_manager_compiled_with_legacy2_sdk());
  bmp = gbitmap_create_blank(s10, GBitmapFormat1Bit);
  cl_assert(NULL != bmp);
  cl_assert(NULL == bmp->data_row_infos);

  bmp = gbitmap_create_blank(s10, GBitmapFormat8Bit);
  cl_assert(NULL == bmp);

  bmp = gbitmap_create_blank(s10, GBitmapFormat1BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat2BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat4BitPalette);
  cl_assert(NULL == bmp);

  bmp = gbitmap_create_blank(s10, GBitmapFormat8BitCircular);
  cl_assert(NULL == bmp);

  bmp = gbitmap_create_blank(s180, GBitmapFormat8BitCircular);
  cl_assert(NULL == bmp);
#endif
#ifdef PLATFORM_SNOWY
  bmp = gbitmap_create_blank(s10, GBitmapFormat1Bit);
  cl_assert(NULL != bmp);
  cl_assert(NULL == bmp->data_row_infos);

  bmp = gbitmap_create_blank(s10, GBitmapFormat8Bit);
  cl_assert(NULL != bmp);
  cl_assert(NULL == bmp->data_row_infos);

  bmp = gbitmap_create_blank(s10, GBitmapFormat1BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat2BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat4BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat8BitCircular);
  cl_assert(NULL == bmp);

  bmp = gbitmap_create_blank(s180, GBitmapFormat8BitCircular);
  cl_assert(NULL == bmp);
#endif
#ifdef PLATFORM_SPALDING
  bmp = gbitmap_create_blank(s10, GBitmapFormat1Bit);
  cl_assert(NULL != bmp);
  cl_assert(NULL == bmp->data_row_infos);

  bmp = gbitmap_create_blank(s10, GBitmapFormat8Bit);
  cl_assert(NULL != bmp);
  cl_assert(NULL == bmp->data_row_infos);

  bmp = gbitmap_create_blank(s10, GBitmapFormat1BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat2BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat4BitPalette);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos != bmp->data_row_infos); // union with .palette

  bmp = gbitmap_create_blank(s10, GBitmapFormat8BitCircular);
  cl_assert(NULL == bmp);

  bmp = gbitmap_create_blank(s180, GBitmapFormat8BitCircular);
  cl_assert(NULL != bmp);
  cl_assert(g_gbitmap_spalding_data_row_infos == bmp->data_row_infos);
#endif
}

void test_gbitmap_formats__create_blank_with_palette(void) {
  const GSize s10 = GSize(10, 10);
  const GSize s180 = GSize(180, 180);
  GBitmap *bmp;
  GColor8 *p = (GColor8 *)&p; // some value to test against

#ifdef PLATFORM_TINTIN
  cl_assert(!process_manager_compiled_with_legacy2_sdk());
  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat1Bit, p, true));
  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat8Bit, p, true));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat1BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat2BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat4BitPalette, p, true);
  cl_assert(NULL == bmp);

  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat8BitCircular, p, true));
  cl_assert(NULL == gbitmap_create_blank_with_palette(s180, GBitmapFormat8BitCircular, p, true));
#endif
#ifdef PLATFORM_SNOWY
  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat1Bit, p, true));
  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat8Bit, p, true));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat1BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat2BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat4BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat8BitCircular, p, true));
  cl_assert(NULL == gbitmap_create_blank_with_palette(s180, GBitmapFormat8BitCircular, p, true));
#endif
#ifdef PLATFORM_SPALDING
  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat1Bit, p, true));
  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat8Bit, p, true));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat1BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat2BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  bmp = gbitmap_create_blank_with_palette(s10, GBitmapFormat4BitPalette, p, true);
  cl_assert(NULL != bmp);
  cl_assert(p == gbitmap_get_palette(bmp));

  cl_assert(NULL == gbitmap_create_blank_with_palette(s10, GBitmapFormat8BitCircular, p, true));
  cl_assert(NULL == gbitmap_create_blank_with_palette(s180, GBitmapFormat8BitCircular, p, true));
#endif
}

void test_gbitmap_formats__display_framebuffer_bytes(void) {
#ifdef PLATFORM_TINTIN
  const size_t expected = 20 * 168; // 20 * 8 == 144px + 2 bytes padding per scanline
#endif
#ifdef PLATFORM_SNOWY
  const size_t expected = 144 * 168;
#endif
#ifdef PLATFORM_SPALDING
  // all pixels + 2*76
  const size_t expected = 25944;
#endif
  cl_assert_equal_i(expected, DISPLAY_FRAMEBUFFER_BYTES);
}

size_t prv_gbitmap_size_for_data(GSize size, GBitmapFormat format);

void test_gbitmap_formats__size_for_data(void) {
  cl_assert_equal_i( 40, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat1Bit));
  cl_assert_equal_i(130, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat8Bit));
  cl_assert_equal_i( 20, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat1BitPalette));
  cl_assert_equal_i( 40, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat2BitPalette));
  cl_assert_equal_i( 70, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat4BitPalette));
  cl_assert_equal_i(  0, prv_gbitmap_size_for_data(GSize(13, 10), GBitmapFormat8BitCircular));

  const size_t expected = PBL_IF_RECT_ELSE(0, DISPLAY_FRAMEBUFFER_BYTES);
  cl_assert_equal_i(expected,
      prv_gbitmap_size_for_data(GSize(180, 180), GBitmapFormat8BitCircular));
  cl_assert_equal_i(expected,
      prv_gbitmap_size_for_data(GSize(DISP_COLS, DISP_ROWS), GBitmapFormat8BitCircular));
}
