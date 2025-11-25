/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/gbitmap_png.h"


#include "clar.h"
#include "util.h"

#include <string.h>

// Stubs
////////////////////////////////////
#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_print.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_serial.h"
#include "stubs_ui_window.h"
#include "stubs_compiled_with_legacy2_sdk.h"

extern uint8_t prv_byte_reverse(uint8_t b);

void test_palettized_conversion__test_byte_reverse(void) {
  cl_assert(prv_byte_reverse(0b11110000) == 0b00001111);
  cl_assert(prv_byte_reverse(0b10100101) == 0b10100101);
}

// Each of the following tests depend on a PNG being converted into 1-bit PBI,
// as well as loading in a b&w PNG, which internally will be converted to a
// 1-bit palettized image.
// The palettized PNG loaded in is to be used as the expected value

#define TEST_1BIT_FILE TEST_PBI_FILE_FMT(1bit)
#define TEST_PALETTIZED_FILE TEST_PBI_FILE

void test_palettized_conversion__create_palettized_from_1bit(void) {
  GBitmap *img_1bit = get_gbitmap_from_pbi(TEST_1BIT_FILE);
  cl_assert(img_1bit);
  GBitmap *img_palettized = gbitmap_create_palettized_from_1bit(img_1bit);
  cl_assert(img_palettized);

  gbitmap_pbi_eq(img_palettized, TEST_PALETTIZED_FILE);

  gbitmap_destroy(img_palettized);

  GRect test_bounds = GRect(3, 3, 46, 46);
  img_1bit->bounds = test_bounds;
  img_palettized = gbitmap_create_palettized_from_1bit(img_1bit);

  cl_assert(gcolor_equal(img_palettized->palette[0], GColorBlack));
  cl_assert(gcolor_equal(img_palettized->palette[1], GColorWhite));

  gbitmap_pbi_eq_with_bounds(img_palettized, TEST_PALETTIZED_FILE, &test_bounds);

  gbitmap_destroy(img_palettized);
  gbitmap_destroy(img_1bit);
}
