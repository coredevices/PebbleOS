/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/rockyjs/api/rocky_api_graphics_path2d.h>
#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/graphics/gpath.h"
#include "applib/graphics/gtypes.h"
#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_graphics.h"
#include "applib/rockyjs/api/rocky_api_graphics_color.h"
#include "applib/rockyjs/pbl_jerry_port.h"
#include "util/trig.h"

// Standard
#include "string.h"

// Fakes
#include "fake_app_timer.h"
#include "fake_logging.h"
#include "fake_pbl_malloc.h"
#include "fake_time.h"

// Stubs
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
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

bool rocky_api_graphics_color_parse(const char *color_value, GColor8 *parsed_color) {
  return false;
}

bool rocky_api_graphics_color_from_value(jerry_value_t value, GColor *result) {
  return false;
}

static Window s_app_window_stack_get_top_window;
Window *app_window_stack_get_top_window() {
  return &s_app_window_stack_get_top_window;
}

GPointPrecise gpoint_from_polar_precise(const GPointPrecise *precise_center,
                                        uint16_t precise_radius, int32_t angle) {
  return GPointPreciseFromGPoint(GPointZero);
}

GContext s_context;

void rocky_api_graphics_text_init(void) {}
void rocky_api_graphics_text_deinit(void) {}
void rocky_api_graphics_text_add_canvas_methods(jerry_value_t obj) {}
void rocky_api_graphics_text_reset_state(void) {}

void graphics_context_set_fill_color(GContext* ctx, GColor color) {}
void graphics_context_set_stroke_color(GContext* ctx, GColor color) {}
void graphics_context_set_stroke_width(GContext* ctx, uint8_t stroke_width) {}

// mocks


static MockCallRecordings s_graphics_line_draw_precise_stroked;
void graphics_line_draw_precise_stroked(GContext* ctx, GPointPrecise p0, GPointPrecise p1) {
  record_mock_call(s_graphics_line_draw_precise_stroked) {
    .ctx = ctx, .pp0 = p0, .pp1 = p1,
  };
}

void graphics_draw_line(GContext* ctx, GPoint p0, GPoint p1) {
  // TODO: remove me PBL-42458 (still used for drawing arc)
  record_mock_call(s_graphics_line_draw_precise_stroked) {.ctx = ctx};
}

MockCallRecordings s_graphics_draw_arc_precise;
void graphics_draw_arc_precise_internal(GContext *ctx, GPointPrecise center, Fixed_S16_3 radius,
                                        int32_t angle_start, int32_t angle_end) {
  record_mock_call(s_graphics_draw_arc_precise) {
    .draw_arc.center = center,
    .draw_arc.radius = radius,
    .draw_arc.angle_start = angle_start,
    .draw_arc.angle_end = angle_end,
  };
}

MockCallRecordings s_gpath_draw_filled;
void gpath_draw_filled(GContext* ctx, GPath *path) {
  record_mock_call(s_gpath_draw_filled) {
    .path.num_points = path->num_points,
  };
  memcpy(s_gpath_draw_filled.last_call.path.points,
         path->points, sizeof(path->points[0]) * path->num_points);
}


void graphics_fill_rect(GContext *ctx, const GRect *rect) {}

void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t corner_radius,
                                       GCornerMask corner_mask) {}

void graphics_draw_rect_precise(GContext *ctx, const GRectPrecise *rect) {}

void graphics_fill_radial_precise_internal(GContext *ctx, GPointPrecise center,
                                           Fixed_S16_3 radius_inner, Fixed_S16_3 radius_outer,
                                           int32_t angle_start, int32_t angle_end) {}

void layer_mark_dirty(Layer *layer) {}

jerry_value_t prv_create_canvas_context_2d_for_layer(Layer *layer);
static void prv_create_global_ctx(void) {
  // make this easily testable by putting it int JS context as global
  Layer l = {.bounds = GRect(0, 0, 144, 168)};
  const jerry_value_t ctx = prv_create_canvas_context_2d_for_layer(&l);
  cl_assert_equal_b(jerry_value_is_object(ctx), true);
  jerry_set_object_field(jerry_get_global_object(), "ctx", ctx);
}

void test_rocky_api_graphics_path2d__initialize(void) {
  fake_malloc_set_largest_free_block(~0);
  s_log_internal__expected = NULL;

  rocky_runtime_context_init();
  fake_app_timer_init();
  jerry_init(JERRY_INIT_EMPTY);

  s_app_window_stack_get_top_window = (Window){};
  s_context = (GContext){};
  s_app_state_get_graphics_context = &s_context;
  s_app_event_loop_callback = NULL;

  s_graphics_line_draw_precise_stroked = (MockCallRecordings){0};
  s_graphics_draw_arc_precise = (MockCallRecordings){0};
  s_gpath_draw_filled = (MockCallRecordings){0};
}

void test_rocky_api_graphics_path2d__cleanup(void) {
  fake_app_timer_deinit();

  // Frees the internal path steps array ():
  rocky_api_graphics_path2d_reset_state();

  // some tests deinitialize the engine, avoid double de-init
  if (app_state_get_rocky_runtime_context() != NULL) {
    jerry_cleanup();
    rocky_runtime_context_deinit();
  }

  fake_pbl_malloc_check_net_allocs();
}

static const RockyGlobalAPI *s_graphics_api[] = {
  &GRAPHIC_APIS,
  NULL,
};

#define PP(x, y) \
  GPointPrecise((int16_t)(((x)) * FIXED_S16_3_FACTOR), \
                (int16_t)(((y)) * FIXED_S16_3_FACTOR))

void test_rocky_api_graphics_path2d__invalid_coords(void) {
  rocky_global_init(s_graphics_api);
  prv_create_global_ctx();

  EXECUTE_SCRIPT("ctx.moveTo(4095.375, -4095.5);");
  EXECUTE_SCRIPT_EXPECT_ERROR("ctx.moveTo(4096.5, 0);", "TypeError: Value out of bounds");
  EXECUTE_SCRIPT_EXPECT_ERROR("ctx.moveTo(0, -4095.625);", "TypeError: Value out of bounds");

  EXECUTE_SCRIPT("ctx.lineTo(4095.375, -4095.5);");
  EXECUTE_SCRIPT_EXPECT_ERROR("ctx.lineTo(4096.5, 0);", "TypeError: Value out of bounds");
  EXECUTE_SCRIPT_EXPECT_ERROR("ctx.lineTo(0, -4095.625);", "TypeError: Value out of bounds");
}

void test_rocky_api_graphics_path2d__minimal_path(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  EXECUTE_SCRIPT(
    "ctx.beginPath();\n"
    "ctx.moveTo(1, 2);\n"
    "ctx.lineTo(3.5, -4.5);\n"
    "ctx.stroke();\n"
  );

  cl_assert_equal_i(1, s_graphics_line_draw_precise_stroked.call_count);
  cl_assert_equal_point_precise(PP(0.5, 1.5), s_graphics_line_draw_precise_stroked.last_call.pp0);
  cl_assert_equal_point_precise(PP(3, -5), s_graphics_line_draw_precise_stroked.last_call.pp1);

  EXECUTE_SCRIPT(
    "ctx.fill();\n"
  );
  cl_assert_equal_i(0, s_gpath_draw_filled.call_count);
}


void test_rocky_api_graphics_path2d__more_lines(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  EXECUTE_SCRIPT(
    "ctx.beginPath();\n"
    "ctx.moveTo(1, 2);\n"
    "ctx.lineTo(3, 4);\n"
    "ctx.lineTo(5, 6);\n"
    "ctx.lineTo(7, 8);\n"
    "ctx.moveTo(9, 10);\n"
    "ctx.lineTo(11, 12);\n"
    "ctx.stroke();\n"
  );

  cl_assert_equal_i(4, s_graphics_line_draw_precise_stroked.call_count);
  cl_assert_equal_point_precise(PP(8.5, 9.5), s_graphics_line_draw_precise_stroked.last_call.pp0);
  cl_assert_equal_point_precise(PP(10.5, 11.5), s_graphics_line_draw_precise_stroked.last_call.pp1);

  EXECUTE_SCRIPT(
    "ctx.fill();\n"
  );
  // only first shape has at least 3 points
  cl_assert_equal_i(1, s_gpath_draw_filled.call_count);
  MockCallRecording *lc = &s_gpath_draw_filled.last_call;
  cl_assert_equal_i(4, lc->path.num_points);
  cl_assert_equal_point(GPoint(0, 1), lc->path.points[0]);
  cl_assert_equal_point(GPoint(2, 3), lc->path.points[1]);
  cl_assert_equal_point(GPoint(4, 5), lc->path.points[2]);
  cl_assert_equal_point(GPoint(6, 7), lc->path.points[3]);
}

void test_rocky_api_graphics_path2d__fill(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  EXECUTE_SCRIPT(
    "ctx.moveTo(1, 2);\n"
    "ctx.lineTo(3, 4);\n"
    "ctx.fill();\n"
  );
  // only 2 points
  cl_assert_equal_i(0, s_gpath_draw_filled.call_count);

  EXECUTE_SCRIPT(
    "ctx.lineTo(5, 6);\n"
    "ctx.fill();\n"
  );
  cl_assert_equal_i(1, s_gpath_draw_filled.call_count);
  MockCallRecording *lc = &s_gpath_draw_filled.last_call;
  cl_assert_equal_i(3, lc->path.num_points);
  cl_assert_equal_point(GPoint(0, 1), lc->path.points[0]);
  cl_assert_equal_point(GPoint(2, 3), lc->path.points[1]);
  cl_assert_equal_point(GPoint(4, 5), lc->path.points[2]);

  s_gpath_draw_filled.call_count = 0;
  EXECUTE_SCRIPT(
    "ctx.moveTo(7, 8);\n"
    "ctx.lineTo(9, 10);\n"
    "ctx.fill();\n"
  );

  // still only the first part (before the .moveTo()) as the second only has two points
  cl_assert_equal_i(1, s_gpath_draw_filled.call_count);
  cl_assert_equal_i(3, lc->path.num_points);
  cl_assert_equal_point(GPoint(0, 1), lc->path.points[0]);
  cl_assert_equal_point(GPoint(2, 3), lc->path.points[1]);
  cl_assert_equal_point(GPoint(4, 5), lc->path.points[2]);

  s_gpath_draw_filled.call_count = 0;
  EXECUTE_SCRIPT(
    "ctx.lineTo(11.5, 12.7);\n"
    "ctx.fill();\n"
  );
  // still only the first part (before the .moveTo()) as the second only has two points
  cl_assert_equal_i(2, s_gpath_draw_filled.call_count);
  cl_assert_equal_i(3, lc->path.num_points);
  cl_assert_equal_point(GPoint(6, 7), lc->path.points[0]);
  cl_assert_equal_point(GPoint(8, 9), lc->path.points[1]);
  cl_assert_equal_point(GPoint(11, 12), lc->path.points[2]);

  EXECUTE_SCRIPT_EXPECT_ERROR(
    "ctx.arc(1, 2, 3, 4, 5);\n"
    "ctx.fill();\n"
  , "TypeError: fill() does not support arc()");
}

void test_rocky_api_graphics_path2d__fill_oom(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  EXECUTE_SCRIPT(
    "ctx.moveTo(1, 2);\n"
    "ctx.lineTo(3, 4);\n"
    "ctx.lineTo(5, 6);\n"
  );

  // OOM!
  fake_malloc_set_largest_free_block(0);

  // Call implementation directly instead of executing a script, to avoid mallocs by the VM itself:
  extern jerry_value_t rocky_api_graphics_path2d_call_fill(void);
  const jerry_value_t error_value = rocky_api_graphics_path2d_call_fill();
  ASSERT_JS_ERROR(error_value, "RangeError: Out of memory: too many points to fill");
}

void test_rocky_api_graphics_path2d__arc(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  EXECUTE_SCRIPT(
    "ctx.beginPath();\n"
      "ctx.moveTo(1, 2);\n"
      "ctx.arc(50, 40, 30, Math.PI, 0);\n"
      "ctx.arc(60, 80.1, 20.5, 0, Math.PI, false);\n"
      "ctx.stroke();\n"
  );

  cl_assert_equal_i(2, s_graphics_line_draw_precise_stroked.call_count);
  cl_assert_equal_i(2, s_graphics_draw_arc_precise.call_count);
  MockCallRecording *lc = &s_graphics_draw_arc_precise.last_call;
  cl_assert_equal_point_precise(PP(59.5, 79.625), lc->draw_arc.center);
  cl_assert_equal_i(20.5 * 8, lc->draw_arc.radius.raw_value);
  cl_assert_equal_i(TRIG_MAX_ANGLE * 1 / 4, lc->draw_arc.angle_start);
  cl_assert_equal_i(TRIG_MAX_ANGLE * 3 / 4, lc->draw_arc.angle_end);
}

void test_rocky_api_graphics_path2d__anti_clockwise(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  EXECUTE_SCRIPT(
    "ctx.beginPath();\n"
    "ctx.moveTo(80, 40);\n"
    "ctx.arc(60, 80, 20, 0, Math.PI, true);\n"
    "ctx.stroke();\n"
  );

  cl_assert_equal_i(1, s_graphics_line_draw_precise_stroked.call_count);

  cl_assert_equal_i(1, s_graphics_draw_arc_precise.call_count);
  MockCallRecording *lc = &s_graphics_draw_arc_precise.last_call;
  cl_assert_equal_point_precise(PP(59.5, 79.5), lc->draw_arc.center);
  cl_assert_equal_i(20 * 8, lc->draw_arc.radius.raw_value);
  cl_assert_equal_i(TRIG_MAX_ANGLE * 3 / 4, lc->draw_arc.angle_start);
  cl_assert_equal_i(TRIG_MAX_ANGLE * 5 / 4, lc->draw_arc.angle_end);
}

void test_rocky_api_graphics_path2d__unsupported(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  EXECUTE_SCRIPT_EXPECT_UNDEFINED("ctx.arcTo");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("ctx.bezierCurveTo");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("ctx.quadraticCurveTo");
}

extern size_t s_rocky_path_steps_num;
extern RockyAPIPathStep *s_rocky_path_steps;


void test_rocky_api_graphics_path2d__state_initialized_between_renders(void) {
  rocky_global_init(s_graphics_api);

  s_rocky_path_steps_num = 2;
  EXECUTE_SCRIPT("_rocky.on('draw', function(e) {});");
  Layer *l = &app_window_stack_get_top_window()->layer;
  l->update_proc(l, NULL);

  cl_assert_equal_i(0, s_rocky_path_steps_num);
}

void test_rocky_api_graphics_path2d__rect(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  cl_assert_equal_i(0, s_rocky_path_steps_num);
  EXECUTE_SCRIPT(
    "ctx.moveTo(1, 2);\n"
    "ctx.rect(3, 4, 5, 6);\n"
  );
  cl_assert_equal_i(6, s_rocky_path_steps_num);

  EXECUTE_SCRIPT(
    "ctx.rect(7, 8, 9, 10);\n"
  );
  cl_assert_equal_i(11, s_rocky_path_steps_num);
  cl_assert_equal_i(RockyAPIPathStepType_MoveTo, s_rocky_path_steps[0].type);
  cl_assert_equal_i(RockyAPIPathStepType_MoveTo, s_rocky_path_steps[1].type);
  cl_assert_equal_i(RockyAPIPathStepType_LineTo, s_rocky_path_steps[5].type);
  cl_assert_equal_i(RockyAPIPathStepType_MoveTo, s_rocky_path_steps[6].type);

  cl_assert_equal_point_precise((GPointPrecise(20, 28)), s_rocky_path_steps[1].pt.xy);
  cl_assert_equal_point_precise((GPointPrecise(60, 76)), s_rocky_path_steps[3].pt.xy);
  cl_assert_equal_point_precise((GPointPrecise(20, 28)), s_rocky_path_steps[5].pt.xy);

  // actual correctness of these values is test in test_rocky_api_graphics_rendering.c
  cl_assert_equal_vector_precise((GVectorPrecise(0, 8)), s_rocky_path_steps[1].pt.fill_delta);
  cl_assert_equal_vector_precise((GVectorPrecise(8, 0)), s_rocky_path_steps[3].pt.fill_delta);
  cl_assert_equal_vector_precise((GVectorPrecise(0, 8)), s_rocky_path_steps[5].pt.fill_delta);
}

void test_rocky_api_graphics_path2d__close_path(void) {
  rocky_global_init(s_graphics_api);

  prv_create_global_ctx();

  cl_assert_equal_i(0, s_rocky_path_steps_num);
  EXECUTE_SCRIPT(
    "ctx.moveTo(1, 2);\n"
    "ctx.closePath();\n"
  );
  cl_assert_equal_i(1, s_rocky_path_steps_num);
  EXECUTE_SCRIPT(
    "ctx.lineTo(3, 4);\n"
    "ctx.closePath();\n"
  );
  cl_assert_equal_i(3, s_rocky_path_steps_num);
  cl_assert_equal_i(RockyAPIPathStepType_LineTo, s_rocky_path_steps[2].type);
  cl_assert_equal_point_precise(GPointPrecise(4, 12), (s_rocky_path_steps[0].pt.xy));
  cl_assert_equal_point_precise(GPointPrecise(4, 12), (s_rocky_path_steps[2].pt.xy));
}

extern jerry_value_t rocky_api_graphics_path2d_try_allocate_steps(size_t increment_steps);
extern size_t rocky_api_graphics_path2d_min_array_len(void);
extern size_t rocky_api_graphics_path2d_array_len(void);

void test_rocky_api_graphics_path2d__initial_increment_larger_than_initial_size(void) {
  cl_assert_equal_i(rocky_api_graphics_path2d_array_len(), 0);
  const size_t min_size = rocky_api_graphics_path2d_min_array_len();
  const jerry_value_t rv = rocky_api_graphics_path2d_try_allocate_steps(min_size + 1);
  ASSERT_JS_ERROR(rv, NULL);
  jerry_release_value(rv);
  const size_t actual_size = rocky_api_graphics_path2d_array_len();
  cl_assert(actual_size >= min_size + 1);
}

void test_rocky_api_graphics_path2d__array_realloc_oom(void) {
  fake_malloc_set_largest_free_block(0);
  const jerry_value_t rv = rocky_api_graphics_path2d_try_allocate_steps(1);
  ASSERT_JS_ERROR(rv, "RangeError: Out of memory: can't create more path steps");
  jerry_release_value(rv);
}
