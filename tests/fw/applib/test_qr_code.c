/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <applib/graphics/framebuffer.h>
#include <applib/graphics/graphics.h>
#include <applib/ui/layer.h>
#include <applib/ui/qr_code.h>
#include <clar.h>
#include <util.h>

// Helper Functions
////////////////////////////////////
#include <8bit/test_framebuffer.h>
#include <test_graphics.h>

// Stubs
////////////////////////////////////
#include <graphics_common_stubs.h>
#include <stubs_applib_resource.h>

#define MAX_DATA_LEN 1274

static FrameBuffer *s_fb;
static GContext s_ctx;
static uint8_t s_data[MAX_DATA_LEN];

void test_qr_code__initialize(void) {
  uint32_t x = 0x12345678;

  for (size_t i = 0; i < MAX_DATA_LEN; i++) {
    x = x * 1103515245 + 12345;
    s_data[i] = x >> 16;
  }

  s_fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(s_fb, &(GSize){DISP_COLS, DISP_ROWS});
  test_graphics_context_init(&s_ctx, s_fb);
  setup_test_aa_sw(&s_ctx, s_fb, GRect(0, 0, DISP_COLS, DISP_ROWS),
                   GRect(0, 0, DISP_COLS, DISP_ROWS), false, 1);
  graphics_context_set_fill_color(&s_ctx, GColorRed);
  graphics_fill_rect(&s_ctx, &GRect(0, 0, DISP_COLS, DISP_ROWS));
}

void test_qr_code__cleanup(void) {
  free(s_fb);
}

static void prv_render(size_t len, QRCodeECC ecc) {
  QRCode *qr_code = qr_code_create(GRect(0, 0, DISP_COLS, DISP_ROWS));

  cl_assert(qr_code != NULL);
  qr_code_set_data(qr_code, s_data, len);
  qr_code_set_ecc(qr_code, ecc);
  layer_render_tree(&qr_code->layer, &s_ctx);
  qr_code_destroy(qr_code);
}

void test_qr_code__short_low(void) {
  prv_render(17, QRCodeECCLow);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_qr_code__medium_medium(void) {
  prv_render(106, QRCodeECCMedium);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_qr_code__boundary_quartile(void) {
  prv_render(152, QRCodeECCQuartile);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_qr_code__long_high(void) {
  prv_render(1273, QRCodeECCHigh);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_qr_code__over_capacity(void) {
  uint8_t *before = malloc(FRAMEBUFFER_SIZE_BYTES);

  cl_assert(before != NULL);
  memcpy(before, s_fb->buffer, FRAMEBUFFER_SIZE_BYTES);
  prv_render(1274, QRCodeECCHigh);
  cl_assert_equal_m(s_fb->buffer, before, FRAMEBUFFER_SIZE_BYTES);
  free(before);
}
