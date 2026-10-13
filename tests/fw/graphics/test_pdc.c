/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util.h"

#include <applib/graphics/gtypes.h>
#include <clar.h>

//// Stubs

// tests
void test_pdc__draw_pdc_image(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PDC_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}
