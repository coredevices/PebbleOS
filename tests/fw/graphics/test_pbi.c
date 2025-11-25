/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/gtypes.h"

#include "clar.h"
#include "util.h"

#include <string.h>
#include <stdio.h>

// Stubs
////////////////////////////////////
#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_heap.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_logging.h"

// Tests
////////////////////////////////////

// Reference PNGs reside in "tests/fw/graphics/test_images/"
// and are created at build time, with the test PBI file generated 
// by bitmapgen.py from the reference PNG copied to TEST_IMAGES_PATH
// covers 1,2,4,8 bit palettized
// covers 1,2,4,8 bit palettized with transparency

// Tests 1-bit red&white palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_1_bit(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

// Tests 2-bit palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_2_bit(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

// Tests 4-bit palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_4_bit(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

// Tests 8-bit color PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_8_bit(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

// Tests 1-bit transparent palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_1_bit_transparent(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

// Tests 2-bit transparent palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_2_bit_transparent(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

// Tests 4-bit transparent palettized PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_4_bit_transparent(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

// Tests 8-bit transparent PBI loading into gbitmap
// Result:
//   - gbitmap matches platform loaded PBI
void test_pbi__color_8_bit_transparent(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}

