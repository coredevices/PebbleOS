/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"

#define DO_NOT_STUB_LEGACY2 1
#include "test_rocky_common.h"

#include "applib/graphics/gtypes.h"
#include "applib/rockyjs/api/rocky_api.h"
#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_graphics.h"
#include "applib/rockyjs/api/rocky_api_graphics_text.h"
#include "applib/rockyjs/pbl_jerry_port.h"
#include "util/trig.h"
#include "applib/graphics/framebuffer.h"

#include "../graphics/util.h"


// Standard
#include "string.h"

// Fakes
#include "fake_app_timer.h"
#include "fake_time.h"

// Stubs
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_resources.h"
#include "stubs_sleep.h"
#include "stubs_serial.h"
#include "stubs_syscalls.h"
#include "stubs_sys_exit.h"

size_t heap_bytes_free(void) {
  return 123456;
}

bool gbitmap_init_with_png_data(GBitmap *bitmap, const uint8_t *data, size_t data_size) {
  return false;
}

bool gbitmap_png_data_is_png(const uint8_t *data, size_t data_size) {
  return false;
}

void layer_get_unobstructed_bounds(const Layer *layer, GRect *bounds_out) {
  *bounds_out = layer->bounds;
}

void layer_mark_dirty(Layer *layer) {}

static Window s_app_window_stack_get_top_window;
Window *app_window_stack_get_top_window() {
  return &s_app_window_stack_get_top_window;
}

// no text rendering in this test
void rocky_api_graphics_text_init(void) {}
void rocky_api_graphics_text_deinit(void) {}
void rocky_api_graphics_text_add_canvas_methods(jerry_value_t obj) {}
void rocky_api_graphics_text_reset_state(void) {}


//GBitmap s_bitmap;
//uint8_t s_bitmap_data[DISPLAY_FRAMEBUFFER_BYTES];
GContext s_context;
FrameBuffer *s_framebuffer;
GBitmap *s_pixels;

static void prv_init_gcontext(GSize size) {
  graphics_context_init(&s_context, s_framebuffer, GContextInitializationMode_App);
  framebuffer_clear(s_framebuffer);
  if (s_pixels) {
    gbitmap_destroy(s_pixels);
  }
  s_pixels = gbitmap_create_blank(size, GBITMAP_NATIVE_FORMAT);
  memset(s_pixels->addr, 0xff, size.h * s_pixels->row_size_bytes);
  s_context.dest_bitmap = *s_pixels;
  s_context.draw_state.clip_box = (GRect){.size = size};
  s_context.draw_state.drawing_box = s_context.draw_state.clip_box;
  s_app_state_get_graphics_context = &s_context;
}

void test_rocky_api_graphics_rendering__initialize(void) {
  fake_app_timer_init();
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);

  s_framebuffer = malloc(sizeof(FrameBuffer));
  framebuffer_init(s_framebuffer, &(GSize) { DISP_COLS, DISP_ROWS });
  s_app_window_stack_get_top_window = (Window){};

  prv_init_gcontext((GSize) { DISP_COLS, DISP_ROWS });
  s_app_event_loop_callback = NULL;
}

void test_rocky_api_graphics_rendering__cleanup(void) {
  fake_app_timer_deinit();

  // some tests deinitialize the engine, avoid double de-init
  if (app_state_get_rocky_runtime_context() != NULL) {
    jerry_cleanup();
    rocky_runtime_context_deinit();
  }
  gbitmap_destroy(s_pixels);
  s_pixels = NULL;
  free(s_framebuffer);
}

static const RockyGlobalAPI *s_graphics_api[] = {
  &GRAPHIC_APIS,
  NULL,
};

jerry_value_t prv_create_canvas_context_2d_for_layer(Layer *layer);

static const jerry_value_t prv_global_init_and_set_ctx(void) {
  rocky_global_init(s_graphics_api);

  // make this easily testable by putting it int JS context as global
  Layer l = {.bounds = GRect(0, 0, 144, 168)};
  const jerry_value_t ctx = prv_create_canvas_context_2d_for_layer(&l);
  jerry_set_object_field(jerry_get_global_object(), "ctx", ctx);

  return ctx;
}


void test_rocky_api_graphics_rendering__lines(void) {
  prv_global_init_and_set_ctx();

  // taken from http://fiddle.jshell.net/9298zub9/2/
  EXECUTE_SCRIPT(
    "var t1 = 10;\n"
    "var b1 = 20.5;\n"
    "var t2 = 30.5;\n"
    "var b2 = 40;\n"
    "    \n"
    "for (var i = 1; i <= 5; i++) {\n"
    "  ctx.beginPath();\n"
    "  var x1 = 20 * i;\n"
    "  var x2 = x1 + 10.5;  \n"
    "  ctx.moveTo(x1, t1);\n"
    "  ctx.lineTo(x1, b1);\n"
    "  ctx.moveTo(x2, t1);\n"
    "  ctx.lineTo(x2, b1);\n"
    "\n"
    "  ctx.moveTo(x1, t2);\n"
    "  ctx.lineTo(x1, b2);\n"
    "  ctx.moveTo(x2, t2);\n"
    "  ctx.lineTo(x2, b2);\n"
    "\n"
    "  ctx.lineWidth = i;\n"
    "  ctx.stroke();\n"
    "}\n"
    "for (var i = 1; i <= 5; i++) {\n"
      "  ctx.beginPath();\n"
      "  var y1 = 40 + i * 20;\n"
      "  var y2 = y1 + 10.5;\n"
      "  ctx.moveTo(t1, y1);\n"
      "  ctx.lineTo(b1, y1);\n"
      "  ctx.moveTo(t1, y2);\n"
      "  ctx.lineTo(b1, y2);\n"
      "  \n"
      "  ctx.moveTo(t2, y1);\n"
      "  ctx.lineTo(b2, y1);\n"
      "  ctx.moveTo(t2, y2);\n"
      "  ctx.lineTo(b2, y2);\n"
      "\n"
      "  ctx.lineWidth = i;\n"
      "  ctx.stroke();\n"
      "}\n"
      "for (var i = 1; i <= 5; i++) {\n"
      "  ctx.beginPath();\n"
      "  var xx = 50;\n"
      "  var yy = 50;\n"
      "  var d = 15 * i;\n"
      "  ctx.moveTo(xx, yy + d);\n"
      "  ctx.lineTo(xx + d, yy);\n"
      "\n"
      "  ctx.lineWidth = i;\n"
      "  ctx.stroke();\n"
      "}"
  );

  const bool eq_result =
    gbitmap_pbi_eq(&s_context.dest_bitmap, TEST_NAMED_PBI_FILE("rocky_rendering_lines"));
  cl_check(eq_result);
}

void test_rocky_api_graphics_rendering__rect(void) {
  prv_init_gcontext(GSize(500, 150));
  prv_global_init_and_set_ctx();

  // taken from http://fiddle.jshell.net/a5gjzb7c/6/
  EXECUTE_SCRIPT(
    "function render(x, y, f) {\n"
      "  f(x + 10,   y + 10, 10, 10);\n"
      "  f(x + 30.2, y + 10, 10, 10.2);\n"
      "  f(x + 50.5, y + 10, 10, 10);\n"
      "  f(x + 70.7, y + 10, 10.5, 10.8);\n"
      "  f(x + 10,   y + 30.5, 10, 10);\n"
      "  f(x + 30.2, y + 30.5, 10, 10.2);\n"
      "  f(x + 50.5, y + 30.5, 10, 10);\n"
      "  f(x + 70.7, y + 30.5, 10.5, 10.8);\n"
      "  \n"
      "  f(x + 90,  y + 10, 0, 0);\n"
      "  f(x + 110, y + 10, 0.5, 0.5);\n"
      "  f(x + 90,  y + 30, -2, -2);\n"
      "  f(x + 110, y + 30, -5.5, -6);\n"
      "}"
      "\n"
      "for (var i = 0; i <= 3; i++) {\n"
      "  ctx.lineWidth = i;\n"
      "  var x = 120 * i;\n"
      "  render(x, 0, ctx[i == 0 ? 'fillRect' : 'strokeRect'].bind(ctx));\n"
      "  render(x, 50, function(x,y,w,h) {\n"
      "    ctx.beginPath();\n"
      "    ctx.rect(x, y, w, h);\n"
      "    ctx[i == 0? 'fill' : 'stroke']();  \n"
      "  });\n"
      "  render(x, 100, function r(x, y, w, h) {\n"
      "    ctx.beginPath();\n"
      "    ctx.moveTo(x, y);\n"
      "    ctx.lineTo(x + w, y);\n"
      "    ctx.lineTo(x + w, y + h);\n"
      "    ctx.lineTo(x, y + h);\n"
      "    ctx.lineTo(x, y);\n"
      "    ctx[i == 0? 'fill' : 'stroke']();  \n"
      "  });\n"
      "}"
  );

  const bool eq_result =
    gbitmap_pbi_eq(&s_context.dest_bitmap, TEST_NAMED_PBI_FILE("rocky_rendering_rect"));
  cl_check(eq_result);
}


void test_rocky_api_graphics_rendering__arc(void) {
  prv_init_gcontext(GSize(500, 300));
  prv_global_init_and_set_ctx();

  // http://fiddle.jshell.net/uopr1ez2/2/
  EXECUTE_SCRIPT(
    "var xx = 200;\n"
    "\n"
    "function f(x, y, r, a1, a2) {\n"
    "  ctx.beginPath();\n"
    "  ctx.arc(x, y, r, a1, a2, false);\n"
    "  ctx.stroke();\n"
    "\n"
    "  ctx.rockyFillRadial(x + xx, y, 0, r, a1, a2);\n"
    "}\n"
    "\n"
    "function g(x, y, a1, a2) {\n"
    "  f(x, y, 5, a1, a2);\n"
    "  f(x, y, 15.5, a1, a2);\n"
    "  f(x, y, 25.2, a1, a2);\n"
    "  f(x, y, 34.8, a1, a2);\n"
    "}\n"
    "\n"
    "function h(x, y, a1, a2) {\n"
    "  for (var i = 0; i < 4; i++) {\n"
    "    ctx.lineWidth = i + 1;\n"
    "    g(x, y + 40 * i, a1, a2);\n"
    "  }\n"
    "}\n"
    "\n"
    "h(2, 2, 0, 0.5 * Math.PI);\n"
    "h(50.5, 2.5, 0, 0.5 * Math.PI);\n"
    "h(100.2, 2.2, 0, 0.5 * Math.PI);\n"
    "h(150.8, 2.8, 0, 0.5 * Math.PI);\n"
    "\n"
    "ctx.lineWidth = 1;\n"
    "f(20, 200, 10, 0, 2 * Math.PI);\n"
    "f(60.5, 200, 10, 0, 2 * Math.PI);\n"
    "f(100.5, 200.5, 10, 0, 2 * Math.PI);\n"
    "f(140, 200.5, 10, 0, 2 * Math.PI);\n"
    "\n"
    "f(20, 240, 11, 0, 2 * Math.PI);\n"
    "f(60.5, 240, 11, 0, 2 * Math.PI);\n"
    "f(100.5, 240.5, 11, 0, 2 * Math.PI);\n"
    "f(140, 240.5, 11, 0, 2 * Math.PI);\n"
    "\n"
    "f(20, 280, 11, 0, -0.5 * Math.PI);"
  );

  const bool eq_result =
    gbitmap_pbi_eq(&s_context.dest_bitmap, TEST_NAMED_PBI_FILE("rocky_rendering_arc"));
  cl_check(eq_result);
}