/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/graphics/gcontext.h"
#include "applib/graphics/graphics_circle.h"
#include "applib/rockyjs/api/rocky_api_util.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/layer.h"

#include "kernel/events.h"
#include "syscall/syscall.h"

#include <clar.h>
#include <util/attributes.h>

#include <string.h>

#define ASSERT_JS_GLOBAL_EQUALS_B(name, value) \
  cl_assert_equal_i(prv_js_global_get_boolean(name), value);

#define ASSERT_JS_GLOBAL_EQUALS_I(name, value) \
  cl_assert_equal_i(prv_js_global_get_double(name), value);

#define ASSERT_JS_GLOBAL_EQUALS_D(name, value) \
  cl_assert_equal_d(prv_js_global_get_double(name), value);

#define ASSERT_JS_GLOBAL_EQUALS_S(name, value) \
  do { \
    char str_buffer[1024] = {}; \
    prv_js_global_get_string(name, str_buffer, sizeof(str_buffer)); \
    cl_assert_equal_s(str_buffer, value); \
  } while(0)

#define JS_GLOBAL_GET_VALUE(name) \
  prv_js_global_get_value(name)

#define ASSERT_JS_ERROR(error_value, expected_error_string) \
  do { \
    const bool expects_error = (expected_error_string) != NULL; \
    const bool is_error = jerry_value_has_error_flag(error_value); \
    if (is_error) { \
      if (expects_error) { \
        jerry_char_t buffer[100] = {0}; \
        jerry_object_to_string_to_utf8_char_buffer(error_value, buffer, sizeof(buffer)); \
        cl_assert_equal_s((char *)(expected_error_string), (char *)buffer); \
      } else {\
        rocky_log_exception("ASSERT_JS_ERROR", error_value); \
        cl_fail("Error value while no error was expected!"); \
      } \
    } else { \
      if (expects_error) { \
        cl_fail("expected error during JS execution did not occur"); \
      } \
    } \
  } while(0)

#define EXECUTE_SCRIPT_EXPECT_UNDEFINED(script) \
  do { \
    JS_VAR rv = jerry_eval((jerry_char_t *)script, \
                            strlen(script), \
                            false /* is_strict */); \
    cl_assert_equal_b(true, jerry_value_is_undefined(rv)); \
  } while(0)


#define EXECUTE_SCRIPT_EXPECT_ERROR(script, expected_error) \
  do { \
    JS_VAR rv = jerry_eval((jerry_char_t *)script, \
                           strlen(script), \
                           false /* is_strict */); \
    ASSERT_JS_ERROR(rv, expected_error); \
  } while(0)

#define EXECUTE_SCRIPT(script) EXECUTE_SCRIPT_EXPECT_ERROR((script), NULL)

#define EXECUTE_SCRIPT_AND_ASSERT_RV_EQUALS_S(script, expected_c_string) \
  do { \
      JS_VAR rv = jerry_eval((jerry_char_t *)script, \
                             strlen(script), false /* is_strict */); \
      ASSERT_JS_ERROR(rv, NULL); \
      JS_VAR rv_string = jerry_value_to_string(rv); \
      jerry_size_t sz = jerry_get_utf8_string_size(rv_string); \
      cl_assert(sz); \
      jerry_char_t *buffer = malloc(sz + 1); \
      memset(buffer, 0, sz + 1); \
      cl_assert(buffer); \
      cl_assert_equal_i(sz, jerry_string_to_utf8_char_buffer(rv_string, buffer, sz)); \
      cl_assert_equal_s((char *)buffer, expected_c_string); \
      free(buffer); \
    } while(0)

#ifndef DO_NOT_STUB_LEGACY2

UNUSED bool process_manager_compiled_with_legacy2_sdk(void) {
  return false;
}

#endif

UNUSED static jerry_value_t prv_js_global_get_value(char *name) {
  JS_VAR global_obj = jerry_get_global_object();
  cl_assert_equal_b(jerry_value_is_undefined(global_obj), false);

  JS_VAR val = jerry_get_object_field(global_obj, name);
  cl_assert_equal_b(jerry_value_is_undefined(val), false);
  return jerry_acquire_value(val);
}

UNUSED static bool prv_js_global_get_boolean(char *name) {
  JS_VAR val = prv_js_global_get_value(name);
  cl_assert(jerry_value_is_boolean(val));
  double rv = jerry_get_boolean_value(val);
  return rv;
}

UNUSED static double prv_js_global_get_double(char *name) {
  JS_VAR val = prv_js_global_get_value(name);
  cl_assert(jerry_value_is_number(val));
  double rv = jerry_get_number_value(val);
  return rv;
}

UNUSED static void prv_js_global_get_string(char *name, char *buffer, size_t buffer_size) {
  JS_VAR val = prv_js_global_get_value(name);
  cl_assert(jerry_value_is_string(val));
  ssize_t num_bytes = jerry_string_to_char_buffer(val, (jerry_char_t *)buffer, buffer_size);
  cl_assert(num_bytes <= buffer_size);
}

void (*s_app_event_loop_callback)(void);

void app_event_loop_common(void) {
  if (s_app_event_loop_callback) {
    s_app_event_loop_callback();
  }
}

#define cl_assert_equal_point(a, b) \
  do { \
    const GPoint __pt_a = (a); \
    const GPoint __pt_b = (b); \
    cl_assert_equal_i(__pt_a.x, __pt_b.x); \
    cl_assert_equal_i(__pt_a.y, __pt_b.y); \
  } while(0)

#define cl_assert_equal_point_precise(a, b) \
  do { \
    const GPointPrecise __pt_a = (a); \
    const GPointPrecise __pt_b = (b); \
    cl_assert_equal_i(__pt_a.x.raw_value, __pt_b.x.raw_value); \
    cl_assert_equal_i(__pt_a.y.raw_value, __pt_b.y.raw_value); \
  } while(0)

#define cl_assert_equal_vector_precise(a, b) \
  do { \
    const GVectorPrecise __a = (a); \
    const GVectorPrecise __b = (b); \
    cl_assert_equal_i(__a.dx.raw_value, __b.dx.raw_value); \
    cl_assert_equal_i(__a.dy.raw_value, __b.dy.raw_value); \
  } while(0)

#define cl_assert_equal_size(a, b) \
  do { \
    const GSize __sz_a = (a); \
    const GSize __sz_b = (b); \
    cl_assert_equal_i(__sz_a.w, __sz_b.w); \
    cl_assert_equal_i(__sz_a.h, __sz_b.h); \
  } while(0)

#define cl_assert_equal_size_precise(a, b) \
  do { \
    const GSizePrecise __sz_a = (a); \
    const GSizePrecise __sz_b = (b); \
    cl_assert_equal_i(__sz_a.w.raw_value, __sz_b.w.raw_value); \
    cl_assert_equal_i(__sz_a.h.raw_value, __sz_b.h.raw_value); \
  } while(0)

#define cl_assert_equal_rect(a, b) \
  do { \
    const GRect __a = (a); \
    const GRect __b = (b); \
    cl_assert_equal_point(__a.origin, __b.origin); \
    cl_assert_equal_size(__a.size, __b.size); \
  } while(0)

#define cl_assert_equal_rect_precise(a, b) \
  do { \
    const GRectPrecise __a = (a); \
    const GRectPrecise __b = (b); \
    cl_assert_equal_point_precise(__a.origin, __b.origin); \
    cl_assert_equal_size_precise(__a.size, __b.size); \
  } while(0)


typedef struct {
  union {
    Layer *layer;
    GContext *ctx;
  };
  union {
    GColor color;
    uint8_t width;
    struct {
      GPoint p0;
      GPoint p1;
    };
    struct {
      GPointPrecise pp0;
      GPointPrecise pp1;
    };
    struct {
      GPointPrecise center;
      Fixed_S16_3 radius;
      int32_t angle_start;
      int32_t angle_end;
    } draw_arc;
    struct {
      GRect rect;
      uint16_t radius;
      GCornerMask corner_mask;
    };
    struct {
      GRectPrecise prect;
    };
    TimeUnits tick_units;
    const char *font_key;
    struct {
      char text[200];
      GRect box;
      GColor color;
    } draw_text;
    struct {
      char text[200];
      GFont font;
      GRect box;
      GTextOverflowMode overflow_mode;
      GTextAlignment alignment;
    } max_used_size;
    struct {
      GPoint points[200];
      size_t num_points;
    } path;
    struct {
      GPointPrecise center;
      Fixed_S16_3 radius_inner;
      Fixed_S16_3 radius_outer;
      int32_t angle_start;
      int32_t angle_end;
    } fill_radial_precise;
  };
} MockCallRecording;

typedef struct {
  int call_count;
  MockCallRecording last_call;
} MockCallRecordings;

#define record_mock_call(var) \
    var.call_count++; \
    var.last_call = (MockCallRecording)

// Handy for poking at .js things when debugging a unit test with gdb, for example:
// (gdb) call js_eval("1 + 1")
// 2
// (gdb) call js_eval("Date()")
// Thu Jan 01 1970 00:00:00 GMT+00:00
void js_eval(const char *src) {
  JS_VAR rv = jerry_eval((const jerry_char_t *)src, strlen(src), false);
  char buf[256] = {};
  jerry_object_to_string_to_utf8_char_buffer(rv, (jerry_char_t *)buf, sizeof(buf));
  printf("%s\n", buf);
}

static void (*s_process_manager_callback)(void *data);
static void *s_process_manager_callback_data;
void sys_current_process_schedule_callback(CallbackEventCallback async_cb,
                                           void *ctx) {
  s_process_manager_callback = async_cb;
  s_process_manager_callback_data = ctx;
}
