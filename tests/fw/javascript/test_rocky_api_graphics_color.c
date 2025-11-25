/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/graphics/gtypes.h"
#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_graphics.h"
#include "applib/rockyjs/api/rocky_api_graphics_color.h"
#include "applib/rockyjs/pbl_jerry_port.h"

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

void layer_get_unobstructed_bounds(const Layer *layer, GRect *bounds_out) {
  *bounds_out = layer->bounds;
}

static Window s_app_window_stack_get_top_window;
Window *app_window_stack_get_top_window() {
  return &s_app_window_stack_get_top_window;
}

GContext s_context;

// mocks
static MockCallRecordings s_graphics_context_set_fill_color;
void graphics_context_set_fill_color(GContext* ctx, GColor color) {
  record_mock_call(s_graphics_context_set_fill_color) {
    .ctx = ctx, .color = color,
  };
}

static MockCallRecordings s_graphics_context_set_stroke_color;
void graphics_context_set_stroke_color(GContext* ctx, GColor color) {
  record_mock_call(s_graphics_context_set_stroke_color) {
    .ctx = ctx, .color = color,
  };
}

void graphics_context_set_stroke_width(GContext* ctx, uint8_t stroke_width) {}

void graphics_draw_line(GContext* ctx, GPoint p0, GPoint p1) {}

void graphics_fill_rect(GContext *ctx, const GRect *rect) {}

void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t corner_radius,
                                       GCornerMask corner_mask) {}

void graphics_draw_rect_precise(GContext *ctx, const GRectPrecise *rect) {}

void graphics_fill_radial_precise_internal(GContext *ctx, GPointPrecise center,
                                           Fixed_S16_3 radius_inner, Fixed_S16_3 radius_outer,
                                           int32_t angle_start, int32_t angle_end) {}

void layer_mark_dirty(Layer *layer) {}

void rocky_api_graphics_path2d_add_canvas_methods(jerry_value_t obj) {}
void rocky_api_graphics_path2d_cleanup(void) {}
void rocky_api_graphics_path2d_reset_state(void) {}
void rocky_api_graphics_text_init(void) {}
void rocky_api_graphics_text_deinit(void) {}
void rocky_api_graphics_text_add_canvas_methods(jerry_value_t obj) {}
void rocky_api_graphics_text_reset_state(void) {}

jerry_value_t prv_rocky_api_graphics_get_canvas_context_2d(void);

void test_rocky_api_graphics_color__initialize(void) {
  fake_app_timer_init();
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);

  s_app_window_stack_get_top_window = (Window){};
  s_context = (GContext){};
  s_app_state_get_graphics_context = &s_context;
  s_app_event_loop_callback = NULL;

  s_graphics_context_set_stroke_color = (MockCallRecordings){0};
  s_graphics_context_set_fill_color = (MockCallRecordings){0};
}

void test_rocky_api_graphics_color__cleanup(void) {
  fake_app_timer_deinit();

  // some tests deinitialize the engine, avoid double de-init
  if (app_state_get_rocky_runtime_context() != NULL) {
    jerry_cleanup();
    rocky_runtime_context_deinit();
  }
}

static const RockyGlobalAPI *s_graphics_api[] = {
  &GRAPHIC_APIS,
  NULL,
};

#define cl_assert_parsed_color(str, expected_color_ptr) do { \
  GColor actual_color = {0}; \
  actual_color.argb = 123; \
  const bool actual_bool = rocky_api_graphics_color_parse(str, &actual_color); \
  if (expected_color_ptr) { \
    cl_assert_equal_b(true, actual_bool); \
    cl_assert_equal_i(((GColor*)(expected_color_ptr))->argb, actual_color.argb); \
  } else { \
    cl_assert_equal_b(false, actual_bool); \
  }\
  } while(0)


void test_rocky_api_graphics_color__parse_names(void) {
  rocky_global_init(s_graphics_api);

  cl_assert_parsed_color("unknown", NULL);
  cl_assert_parsed_color("clear", &GColorClear);
  cl_assert_parsed_color("black", &GColorBlack);
  cl_assert_parsed_color("red", &GColorRed);
  cl_assert_parsed_color("white", &GColorWhite);
  cl_assert_parsed_color("gray", &GColorLightGray);
}

extern const RockyAPIGraphicsColorDefinition s_color_definitions[];


void test_rocky_api_graphics_color__color_names_consistent(void) {
  rocky_global_init(s_graphics_api);

  const RockyAPIGraphicsColorDefinition *def = s_color_definitions;
  while (def->name) {
    GColor8 actual;
    const bool result = rocky_api_graphics_color_parse(def->name, &actual);
    cl_assert_equal_b(true, result);
    cl_assert_equal_i(def->value, actual.argb);
    def++;
  }
}

void test_rocky_api_graphics_color__hex(void) {
  // invalid cases
  cl_assert_parsed_color("#", NULL);
  cl_assert_parsed_color("##q3", NULL);
  cl_assert_parsed_color("", NULL);
  cl_assert_parsed_color("#00zz10", NULL);
  cl_assert_parsed_color("#123456789", NULL);

  // different lengths
  cl_assert_parsed_color("#f00", &GColorRed);
  cl_assert_parsed_color("#FF0000", &GColorRed);
  cl_assert_parsed_color("#F00f", &GColorRed);
  cl_assert_parsed_color("#FF0000FF", &GColorRed);

  // discard rgb components if alpha == 0
  cl_assert_parsed_color("#12345600", &GColorClear);
  cl_assert_parsed_color("#1230", &GColorClear);

  // correctly assign different components
  cl_assert_parsed_color("#00FF00", &GColorGreen);
  cl_assert_parsed_color("#0000FF", &GColorBlue);

}