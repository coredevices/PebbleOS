/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/graphics/gpath.h"
#include "applib/preferred_content_size.h"
#include "applib/rockyjs/api/rocky_api.h"
#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_timers.h"
#include "applib/rockyjs/api/rocky_api_graphics.h"
#include "applib/rockyjs/api/rocky_api_tickservice.h"
#include "applib/rockyjs/api/rocky_api_util.h"
#include "applib/rockyjs/pbl_jerry_port.h"

#include "syscall/syscall.h"

// Standard
#include "string.h"
#include "applib/rockyjs/rocky.h"

// Fakes
#include "fake_app_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_time.h"
#include "fake_logging.h"

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

const RockyGlobalAPI APP_MESSAGE_APIS = {};
const RockyGlobalAPI WATCHINFO_APIS = {};

size_t heap_bytes_free(void) {
  return 123456;
}

void sys_analytics_inc(AnalyticsMetric metric, AnalyticsClient client) {
}

bool sys_get_current_app_is_rocky_app(void) {
  return true;
}

void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler) {}

static Window s_app_window_stack_get_top_window;
Window *app_window_stack_get_top_window() {
  return &s_app_window_stack_get_top_window;
}

PreferredContentSize preferred_content_size(void) {
  return PreferredContentSizeMedium;
}

static MockCallRecordings s_layer_mark_dirty;
void layer_mark_dirty(Layer *layer) {
  s_layer_mark_dirty.call_count++;
  s_layer_mark_dirty.last_call = (MockCallRecording){.layer = layer};
}

static MockCallRecordings s_graphics_context_set_fill_color;
void graphics_context_set_fill_color(GContext* ctx, GColor color) {
  s_graphics_context_set_fill_color.call_count++;
  s_graphics_context_set_fill_color.last_call = (MockCallRecording){.ctx = ctx, .color = color};
}

static MockCallRecordings s_graphics_context_set_stroke_color;
void graphics_context_set_stroke_color(GContext* ctx, GColor color) {
  s_graphics_context_set_stroke_color.call_count++;
  s_graphics_context_set_stroke_color.last_call = (MockCallRecording){.ctx = ctx, .color = color};
}

static MockCallRecordings s_graphics_context_set_stroke_width;
void graphics_context_set_stroke_width(GContext* ctx, uint8_t stroke_width) {
  s_graphics_context_set_stroke_width.call_count++;
  s_graphics_context_set_stroke_width.last_call =
    (MockCallRecording){.ctx = ctx, .width =stroke_width};
}


static MockCallRecordings s_graphics_line_draw_precise_stroked;
void graphics_line_draw_precise_stroked(GContext* ctx, GPointPrecise p0, GPointPrecise p1) {
  record_mock_call(s_graphics_line_draw_precise_stroked) {
    .ctx = ctx, .pp0 = p0, .pp1 = p1,
  };
}

static MockCallRecordings s_graphics_draw_line;
void graphics_draw_line(GContext* ctx, GPoint p0, GPoint p1) {
  s_graphics_draw_line.call_count++;
  s_graphics_draw_line.last_call =
    (MockCallRecording){.ctx = ctx, .p0 = p0, .p1 = p1};
}

static MockCallRecordings s_graphics_fill_rect;
void graphics_fill_rect(GContext *ctx, const GRect *rect) {
  s_graphics_fill_rect.call_count++;
  s_graphics_fill_rect.last_call = (MockCallRecording){.ctx = ctx, .rect = *rect};
}

static MockCallRecordings s_graphics_fill_rect;
void graphics_fill_round_rect_by_value(GContext* ctx, GRect rect, uint16_t radius,
                                       GCornerMask corner_mask) {
  s_graphics_fill_rect.call_count++;
  s_graphics_fill_rect.last_call = (MockCallRecording) {
    .ctx = ctx,
    .rect = rect,
    .radius = radius,
    .corner_mask = corner_mask,
  };
}

GPointPrecise gpoint_from_polar_precise(const GPointPrecise *precise_center,
                                        uint16_t precise_radius, int32_t angle) {
  return GPointPreciseFromGPoint(GPointZero);
}

void graphics_draw_arc_precise_internal(GContext *ctx, GPointPrecise center, Fixed_S16_3 radius,
                                        int32_t angle_start, int32_t angle_end) {}

void graphics_draw_rect_precise(GContext *ctx, const GRectPrecise *rect) {}

void graphics_fill_radial_precise_internal(GContext *ctx, GPointPrecise center,
                                           Fixed_S16_3 radius_inner, Fixed_S16_3 radius_outer,
                                           int32_t angle_start, int32_t angle_end) {}

void gpath_draw_filled(GContext* ctx, GPath *path) {}

void layer_get_unobstructed_bounds(const Layer *layer, GRect *bounds_out) {
  *bounds_out = layer->bounds;
}

GFont fonts_get_system_font(const char *font_key) {
  return (GFont)123;
}

void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) {}

void graphics_text_attributes_destroy(GTextAttributes *text_attributes) {}

GSize graphics_text_layout_get_max_used_size(GContext *ctx, const char *text,
                                             GFont const font, const GRect box,
                                             const GTextOverflowMode overflow_mode,
                                             const GTextAlignment alignment,
                                             GTextLayoutCacheRef layout) {
  return GSizeZero;
}

uint32_t resource_storage_get_num_entries(ResAppNum app_num, uint32_t resource_id) {
  return 0;
}

static bool s_skip_mem_leak_check;

static void prv_init(void) {
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
}

static void prv_deinit(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
}

void test_js__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  s_skip_mem_leak_check = false;

  fake_app_timer_init();
  prv_init();
  s_app_window_stack_get_top_window = (Window){};
  s_app_state_get_graphics_context = NULL;

  s_layer_mark_dirty = (MockCallRecordings){};
  s_graphics_context_set_fill_color = (MockCallRecordings){};
  s_graphics_context_set_stroke_color = (MockCallRecordings){};
  s_graphics_context_set_stroke_width = (MockCallRecordings){};
  s_graphics_line_draw_precise_stroked = (MockCallRecordings){};
  s_graphics_draw_line = (MockCallRecordings){};
  s_graphics_fill_rect = (MockCallRecordings){};

  s_app_event_loop_callback = NULL;
  s_log_internal__expected = NULL;
  s_app_heap_analytics_log_stats_to_app_heartbeat_call_count = 0;
  s_app_heap_analytics_log_rocky_heap_oom_fault_call_count = 0;
}

void test_js__cleanup(void) {
  fake_app_timer_deinit();
  s_log_internal__expected = NULL;

  // some tests deinitialize the engine, avoid double de-init
  if (app_state_get_rocky_runtime_context() != NULL) {
    prv_deinit();
  }

  // PBL-40702: test_js__init_deinit is leaking memory...
  if (!s_skip_mem_leak_check) {
     fake_pbl_malloc_check_net_allocs();
  }
}

void test_js__addition(void) {
  char script[] = "var a = 1; var b = 2; var c = a + b;";
  EXECUTE_SCRIPT(script);
  ASSERT_JS_GLOBAL_EQUALS_I("c", 3.0);
}

void test_js__eval_error(void) {
  prv_deinit();  // engine will be re-initialized in rocky_event_loop~
  char script[] = "function f({;";
  s_log_internal__expected = (const char *[]){
    "Not a snapshot, interpreting buffer as JS source code",
    "Exception while Evaluating JS",
    "SyntaxError: Identifier expected. [line: 1, column: 12]",
    NULL };
  rocky_event_loop_with_string_or_snapshot(script, sizeof(script));
  cl_assert(*s_log_internal__expected == NULL);
}

AppTimer *rocky_timer_get_app_timer(void *data);

void test_js__init_deinit(void) {
  // PBL-40702: test_js__init_deinit is leaking memory...
  s_skip_mem_leak_check = true;

  prv_deinit();

  char *script =
    "var num_times = 0;"
    "var extra_arg = 0;"
    "var timer = setInterval(function(extra) {"
      "num_times++;"
      "extra_arg = extra;"
    "}, 1000, 5);";

  for (int i = 0; i < 30; ++i) {
    prv_init();
    TIMER_APIS.init();
    EXECUTE_SCRIPT(script);
    prv_deinit();
  }

  prv_init();
}

static char *prv_load_js(char *suffix) {
  char path[512] = {0};
  snprintf(path, sizeof(path), "%s/js/tictoc~rect~%s.js", CLAR_FIXTURE_PATH, suffix);
  FILE *f = fopen(path, "r");
  cl_assert(f);
  fseek(f, 0, SEEK_END);
  size_t length = (size_t)ftell(f);
  fseek (f, 0, SEEK_SET);
  char *buffer = malloc(length + 1);
  memset(buffer, 0, length + 1);
  cl_assert(buffer);
  fread (buffer, 1, length, f);
  fclose (f);
  return buffer;
}

void test_js__call_cleanup_twice(void) {
  prv_deinit();
  char *script = "function f(i) { return i * 4; } f(5);";
  bool result = rocky_event_loop_with_string_or_snapshot(script, strlen(script));
  cl_assert(result);
}

static bool s_tictoc_callback_is_color;
static void prv_rocky_tictoc_callback(void) {
  Layer *root_layer = &s_app_window_stack_get_top_window.layer;
  root_layer->bounds = GRect(10, 20, 30, 40);
  cl_assert(root_layer->update_proc);
  GContext ctx = {.lock = true};
  root_layer->update_proc(root_layer, &ctx);

  if (s_tictoc_callback_is_color) {
    cl_assert_equal_i(1, s_graphics_fill_rect.call_count);
    cl_assert_equal_i(4, s_graphics_line_draw_precise_stroked.call_count);
    cl_assert_equal_i(0, s_graphics_draw_line.call_count);
    cl_assert_equal_i(1, s_graphics_context_set_fill_color.call_count);
    cl_assert_equal_i(4, s_graphics_context_set_stroke_color.call_count);
    cl_assert_equal_i(4, s_graphics_context_set_stroke_width.call_count);
  } else {
    cl_assert_equal_i(2, s_graphics_fill_rect.call_count);
    cl_assert_equal_i(0, s_graphics_line_draw_precise_stroked.call_count);
    cl_assert_equal_i(0, s_graphics_draw_line.call_count);
    cl_assert_equal_i(1, s_graphics_context_set_fill_color.call_count);
    cl_assert_equal_i(0, s_graphics_context_set_stroke_color.call_count);
    cl_assert_equal_i(0, s_graphics_context_set_stroke_width.call_count);
  }

  // run update proc multiple times to verify we don't have a memory leak
  for (int i = 1024; i >=0; i--) {
    root_layer->update_proc(root_layer, &ctx);
  }
}

void test_js__rocky_tictoc_color(void) {
  prv_deinit();  // engine will be re-initialized in rocky_event_loop~
  char *script = prv_load_js("color");
  s_tictoc_callback_is_color = true;
  s_app_event_loop_callback = prv_rocky_tictoc_callback;
  bool result = rocky_event_loop_with_string_or_snapshot(script, strlen(script));
  cl_assert(result);
}

void test_js__rocky_tictoc_bw(void) {
  GContext ctx = {};
  s_app_state_get_graphics_context = &ctx;
  prv_deinit();  // engine will be re-initialized in rocky_event_loop~
  char *script = prv_load_js("bw");
  s_tictoc_callback_is_color = false;
  s_app_event_loop_callback = prv_rocky_tictoc_callback;
  bool result = rocky_event_loop_with_string_or_snapshot(script, strlen(script));
  cl_assert(result);
}

void test_js__recursion(void) {
  const char script[] =
    "function f(i) { \n"
    "  if (i == 0) {_rocky.requestDraw();} \n"
    "  else {f(i-1)}\n"
    "}\n"
    "f(10)";
  static const RockyGlobalAPI *apis[] = {
    &GRAPHIC_APIS,
    NULL,
  };
  rocky_global_init(apis);
  EXECUTE_SCRIPT(script);

  cl_assert_equal_i(1, s_layer_mark_dirty.call_count);
}

void test_js__no_print_builtin(void) {
  JS_VAR global_obj = jerry_get_global_object();
  JS_VAR print_builtin = jerry_get_object_field(global_obj, "print");
  cl_assert_equal_b(true, jerry_value_is_undefined(print_builtin));
}

void test_js__sin_cos(void) {
  EXECUTE_SCRIPT(
    "var s1 = 100 + 50 * Math.sin(0);\n"
    "var s2 = 100 + 50 * Math.sin(2 * Math.PI);\n"
    "var c1 = 100 + 50 * Math.cos(0);\n"
    "var c2 = 100 + 50 * Math.cos(2 * Math.PI);\n"
  );
  cl_assert_equal_i(100, (int32_t)jerry_get_number_value(prv_js_global_get_value("s1")));
  cl_assert_equal_i(99, (int32_t)jerry_get_number_value(prv_js_global_get_value("s2")));
  cl_assert_equal_i(150, (int32_t)jerry_get_number_value(prv_js_global_get_value("c1")));
  cl_assert_equal_i(150, (int32_t)jerry_get_number_value(prv_js_global_get_value("c2")));

  cl_assert_equal_i(100, jerry_get_int32_value(prv_js_global_get_value("s1")));
  cl_assert_equal_i(100, jerry_get_int32_value(prv_js_global_get_value("s2")));
  cl_assert_equal_i(150, jerry_get_int32_value(prv_js_global_get_value("c1")));
  cl_assert_equal_i(150, jerry_get_int32_value(prv_js_global_get_value("c2")));
}

void test_js__date(void) {
  const time_t cur_time = 1458250851; // Thu Mar 17 21:40:51 2016 UTC
                                      // Thu Mar 17 14:40:51 2016 PDT
  const uint16_t cur_millis = 123;
  fake_time_init(cur_time, cur_millis);
  fake_time_set_gmtoff(-8 * 60 * 60); // PST
  fake_time_set_dst(1 * 60 * 60, 1458111600, 1465628400); // PDT 3/16 -> 11/6 2016

  char *script =
    "var date_now = new Date();"
    "var now = date_now.getTime();"
    "var local_day = date_now.getDay();"
    "var local_hour = date_now.getHours();";
  EXECUTE_SCRIPT(script);

  ASSERT_JS_GLOBAL_EQUALS_D("now", (double)cur_time * 1000.0 + (double)cur_millis);
  ASSERT_JS_GLOBAL_EQUALS_D("local_day", 4.0); // Thursday
  ASSERT_JS_GLOBAL_EQUALS_D("local_hour", 14.0); // 1pm
}

void test_js__log_exception(void) {
  char *script =
    "var e1;\n"
    "var f1 = function(){throw new Error('test')};\n"
    "var f2 = function(){throw new 'test';};\n"
    "var f2 = function(){throw new 123;};\n"
    "try {f1();} catch(e) {e1 = e;}\n"
    "try {f2();} catch(e) {e2 = e;}\n"
    "try {f3();} catch(e) {e3 = e;}\n";

  EXECUTE_SCRIPT(script);
  jerry_value_t e1 = prv_js_global_get_value("e1");
  jerry_value_t e2 = prv_js_global_get_value("e2");
  jerry_value_t e3 = prv_js_global_get_value("e3");

  // error
  s_log_internal__expected = (const char *[]){
    "Exception while e1", "Error: test", NULL,
  };
  rocky_log_exception("e1", e1);
  cl_assert(*s_log_internal__expected == NULL);

  // string
  s_log_internal__expected = (const char *[]){
    "Exception while e2", "TypeError", NULL,
  };
  rocky_log_exception("e2", e2);
  cl_assert(*s_log_internal__expected == NULL);

  // number
  s_log_internal__expected = (const char *[]){
    "Exception while e3", "ReferenceError", NULL,
  };
  rocky_log_exception("e3", e3);
  cl_assert(*s_log_internal__expected == NULL);
}

/*
 * FIXME: JS Tests should be built in a 32-bit env
void test_js__size(void) {
  cl_assert_equal_i(4, sizeof(size_t));
}
*/

void test_js__snapshot(void) {
  prv_deinit();
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_SHOW_OPCODES);
  char *const script = prv_load_js("color");
  uint8_t snapshot[65536] = { 0 };

  // make sure snapshot data starts with expected Rocky header
  const size_t header_size = sizeof(ROCKY_EXPECTED_SNAPSHOT_HEADER);
  cl_assert_equal_i(8, header_size);
  // NOTE: the snapshot header in this unit test is fixed to
  //       CAPABILITY_JAVASCRIPT_BYTECODE_VERSION=1 only use the resulting binary
  //       if the true JS version matches
  memcpy(snapshot, &ROCKY_EXPECTED_SNAPSHOT_HEADER, header_size);
  const size_t snapshot_size = jerry_parse_and_save_snapshot((const jerry_char_t *)script,
                                                             strlen(script),
                                                             true,  /* is_for_global */
                                                             false, /* is_strict */
                                                             snapshot + header_size,
                                                             sizeof(snapshot) - header_size);
  cl_assert(snapshot_size > 512); // make sure it contains "something" and compiling didn't fail

  prv_deinit();

  bool result = rocky_event_loop_with_string_or_snapshot(snapshot, snapshot_size);
  cl_assert(result);
}

static int s_cleanup_calls;
static void prv_cleanup_cb(const uintptr_t native_p) {
  ++s_cleanup_calls;
}

void test_js__js_value_cleanup(void) {
  s_cleanup_calls = 0;

  {
    // Sanity check:
    // we don't clean up when a bare jerry_value_t goes out of scope.
    jerry_value_t value = jerry_create_object();
    jerry_set_object_native_handle(value, 0, prv_cleanup_cb);
  }
  jerry_gc(); // Perform GC in case refcount = 0
  cl_assert_equal_i(s_cleanup_calls, 0); // Never release()d, so wasn't cleaned.

  {
    // When this goes out of scope, we do clean up
    JS_VAR value = jerry_create_object();
    jerry_set_object_native_handle(value, 0, prv_cleanup_cb);
  }
  jerry_gc(); // Perform GC so it will be cleaned up if refcount = 0
  cl_assert_equal_i(s_cleanup_calls, 1); // Make sure that it was cleaned up

  {
    // Create a regular value, attach the native handle
    jerry_value_t value = jerry_create_object();
    jerry_set_object_native_handle(value, 0, prv_cleanup_cb);

    // Create an autoreleased variable that points to the same, it will be cleaned up.
    JS_UNUSED_VAL = value;
  }
  jerry_gc();
  cl_assert_equal_i(s_cleanup_calls, 2);

  {
    // Naming check on unused variables, shouldn't clash.
    // This is really just a compile-time test.
    JS_UNUSED_VAL = jerry_create_object();
    JS_UNUSED_VAL = jerry_create_object();
  }
}

void test_js__get_global_builtin(void) {
  jerry_value_t date_builtin = jerry_get_global_builtin((const jerry_char_t *)"Date");
  cl_assert(!jerry_value_is_undefined(date_builtin));
  cl_assert(jerry_value_is_constructor(date_builtin));
  jerry_release_value(date_builtin);

  jerry_value_t json_builtin = jerry_get_global_builtin((const jerry_char_t *)"JSON");
  cl_assert(jerry_value_is_object(json_builtin));
  jerry_release_value(json_builtin);

  jerry_value_t not_builtin = jerry_get_global_builtin((const jerry_char_t *)"_not_builtin_");
  cl_assert(jerry_value_is_undefined(not_builtin));
}

void test_js__get_global_builtin_compare(void) {
  jerry_value_t date_builtin = jerry_get_global_builtin((const jerry_char_t *)"Date");
  jerry_value_t global_object = jerry_get_global_object();

  // Compare that the global Date is the same object as the builtin
  jerry_value_t global_date = jerry_get_object_field(global_object, "Date");
  cl_assert(date_builtin == global_date);

  jerry_release_value(global_date);
  jerry_release_value(global_object);
  jerry_release_value(date_builtin);
}

void test_js__get_global_builtin_changed(void) {
  jerry_value_t date_builtin = jerry_get_global_builtin((const jerry_char_t *)"Date");
  jerry_value_t global_object = jerry_get_global_object();

  const char *source = "Date = 'some string';";
  jerry_eval((const jerry_char_t *)source, strlen(source), false);

  // After changing the global date object, it should not match our builtin
  jerry_value_t global_date = jerry_get_object_field(global_object, "Date");
  cl_assert(jerry_value_is_string(global_date));
  cl_assert(date_builtin != global_date);

  jerry_release_value(global_date);
  jerry_release_value(global_object);
  jerry_release_value(date_builtin);
}

void test_js__capture_mem_stats_upon_exiting_event_loop(void) {
  prv_deinit();

  s_app_event_loop_callback = NULL;
  const char *source = ";";
  cl_assert_equal_b(true, rocky_event_loop_with_string_or_snapshot(source, strlen(source)));
  cl_assert_equal_i(s_app_heap_analytics_log_stats_to_app_heartbeat_call_count, 1);
}

void test_js__jmem_heap_stats_largest_free_block_bytes(void) {
  jmem_heap_stats_t stats = {};
  jmem_heap_get_stats(&stats);
  // Note: this might fail in the future if JerryScript would happen to cause fragmentation right
  // upon initializing the engine:
  cl_assert_equal_i(stats.size - stats.allocated_bytes, stats.largest_free_block_bytes);
}

void test_js__capture_jerry_heap_oom_stats(void) {
  const char *source = "var big = []; for (;;) { big += 'bigger'; };";
  cl_assert_passert(jerry_eval((const jerry_char_t *)source, strlen(source), false));
  cl_assert_equal_i(s_app_heap_analytics_log_rocky_heap_oom_fault_call_count, 1);
}
