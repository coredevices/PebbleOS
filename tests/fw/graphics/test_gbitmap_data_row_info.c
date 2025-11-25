/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gtypes.h"

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

void test_gbitmap_data_row_info__get_info_rectangular(void) {
  uint8_t some_addr;
  GBitmap bmp = {
      .addr = &some_addr,
      .row_size_bytes = 123,
      .bounds = GRect(1, 2, 3, 4),
      .info.format = GBitmapFormat8Bit,
  };

  cl_assert_equal_i(123, gbitmap_format_get_row_size_bytes(123, GBitmapFormat8Bit));
  cl_assert_equal_i(123, gbitmap_get_bytes_per_row(&bmp));
  cl_assert_equal_p(&some_addr + 0 * 123, gbitmap_get_data_row_info(&bmp, 0).data);
  cl_assert_equal_p(&some_addr + 1 * 123, gbitmap_get_data_row_info(&bmp, 1).data);
  cl_assert_equal_i(0, gbitmap_get_data_row_info(&bmp, 3).min_x);
  cl_assert_equal_i(3, gbitmap_get_data_row_info(&bmp, 3).max_x);
}

void test_gbitmap_data_row_info__get_info_circular(void) {
  uint8_t some_addr;
  GBitmapDataRowInfoInternal infos[] = {
      {.offset =  1, .min_x =  2, .max_x =  3}, // 0
      {.offset =  4, .min_x =  5, .max_x =  6}, // 1
      {.offset =  7, .min_x =  8, .max_x =  9}, // 2  // 0
      {.offset = 10, .min_x = 11, .max_x = 12}, // 3  // 1
      {.offset = 13, .min_x = 14, .max_x = 15}, // 4  // 2
      {.offset = 16, .min_x = 17, .max_x = 18}, // 5  // 3
  };
  GBitmap bmp = {
      .addr = &some_addr,
      .row_size_bytes = 123,
      .bounds = GRect(1, 2, 3, 4),
      .info.format = GBitmapFormat8BitCircular,
      .data_row_infos = infos,
  };

  cl_assert_equal_i(0, gbitmap_format_get_row_size_bytes(123, GBitmapFormat8BitCircular));
  cl_assert_equal_i(123, gbitmap_get_bytes_per_row(&bmp));
  cl_assert_equal_p(&some_addr + 1, gbitmap_get_data_row_info(&bmp, 0).data);
  cl_assert_equal_p(&some_addr + 4, gbitmap_get_data_row_info(&bmp, 1).data);
  cl_assert_equal_i(11, gbitmap_get_data_row_info(&bmp, 3).min_x);
  cl_assert_equal_i(12, gbitmap_get_data_row_info(&bmp, 3).max_x);
}
