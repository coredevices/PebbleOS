/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"

#include "applib/ui/window_private.h"
#include "applib/ui/layer.h"
#include "util/graphics.h"

#include "clar.h"
#include "util.h"

#include <stdio.h>

// Helper Functions
////////////////////////////////////
#include "test_graphics.h"
#include "8bit/test_framebuffer.h"

// Stubs
////////////////////////////////////
#include "graphics_common_stubs.h"
#include "stubs_applib_resource.h"

static FrameBuffer *fb = NULL;

#define COLOR(x) ((GColor){.argb=GColor ## x ## ARGB8})
#define NUM_COLORS 4
GColor color_table[NUM_COLORS] = {
  COLOR(Red), 
  COLOR(Yellow),
  COLOR(Cyan),
  COLOR(Black),
};

// Setup
void test_framebuffer_duma__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) { DISP_COLS, DISP_ROWS });
}

// Teardown
void test_framebuffer_duma__cleanup(void) {
  free(fb);
}

// Intentionally unchecked framebuffer drawing function
static void draw_fb_raw(uint8_t *buffer, int offset, GColor8 color) {
  buffer[offset] = color.argb;
}

// Tests
////////////////////////////////////

void test_framebuffer_duma__draw_within_framebuffer(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // This test should only be running on color displays
  cl_assert(ctx.dest_bitmap.info.format == GBitmapFormat8Bit ||
            ctx.dest_bitmap.info.format == GBitmapFormat8BitCircular);

  // This should touch all valid bytes in the framebuffer
  for (int y = DISP_ROWS - 1; y >= 0; y--) {
    GColor color = color_table[y % NUM_COLORS];
    int16_t row_offset = DISP_COLS * y;
    GBitmapDataRowInfo row_info = gbitmap_get_data_row_info(&ctx.dest_bitmap, y);
    for (int x = row_info.min_x; x < row_info.max_x; x++) {
      // Use direct framebuffer access to prove framebuffer-correct positioning
      row_info.data[x] = color.argb;
    }
  }
}
  
// This test validates that a duma assert is caught when drawing outside of the framebuffer
void test_framebuffer_duma__draw_beyond_framebuffer(void) {
  GContext ctx;
  test_graphics_context_init(&ctx, fb);

  // This test should only be running on color displays
  cl_assert(ctx.dest_bitmap.info.format == GBitmapFormat8Bit ||
            ctx.dest_bitmap.info.format == GBitmapFormat8BitCircular);

  uint8_t *buffer = (uint8_t*)ctx.dest_bitmap.addr;
  // Expect this to assert using duma protection, we are writing past framebuffer
  cl_assert_passert(draw_fb_raw(buffer, FRAMEBUFFER_SIZE_BYTES + 1, GColorWhite));
}
