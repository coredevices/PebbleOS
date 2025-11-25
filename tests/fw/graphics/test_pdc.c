/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util.h"

#include "applib/graphics/gtypes.h"

//// Stubs
#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_heap.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_logging.h"

// tests
void test_pdc__draw_pdc_image(void) {
  GBitmap *bitmap = setup_pbi_test(TEST_PDC_PBI_FILE);
  cl_assert(gbitmap_pbi_eq(bitmap, TEST_PBI_FILE));
}
