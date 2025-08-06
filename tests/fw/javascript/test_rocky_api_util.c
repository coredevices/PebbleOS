/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "test_jerry_port_common.h"
#include "test_rocky_common.h"
#include "applib/rockyjs/pbl_jerry_port.h"
#include "../../third_party/jerryscript/jerryscript/jerry-core/ecma/builtin-objects/ecma-builtin-helpers.h"

#include "applib/rockyjs/api/rocky_api_global.h"

#include <clar.h>
#include <math.h>

// Fakes
#include "fake_logging.h"
#include "fake_pbl_malloc.h"
#if EMSCRIPTEN
#include "fake_time_timeshift_js.h"
#else
#include "fake_time.h"
#endif

// Stubs
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_serial.h"
#include "stubs_sys_exit.h"


#define FUNC_NAME "f"
#define ERROR_STRING "Oops!"

T_STATIC void prv_log_uncaught_error(const jerry_value_t result);

static int s_test_func_imp_call_count;
static int s_method_func_imp_call_count;

void tick_timer_service_handle_time_change(void) {}

////////////////////////////////////////////////////////////////////////////////
// Initialization & Setup
////////////////////////////////////////////////////////////////////////////////

void test_rocky_api_util__initialize(void) {
  s_test_func_imp_call_count = 0;
  s_method_func_imp_call_count = 0;
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  s_log_internal__expected = NULL;
}

void test_rocky_api_util__cleanup(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
  fake_pbl_malloc_check_net_allocs();
  s_log_internal__expected = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// Helpers for Tests
////////////////////////////////////////////////////////////////////////////////

static void prv_do_call_user_function(const char *script) {
  const jerry_value_t rv = jerry_eval((jerry_char_t *)script, strlen(script),
                                      false /* is_strict */);
  cl_assert_equal_b(jerry_value_has_error_flag(rv), false);
  jerry_release_value(rv);

  const jerry_value_t func = JS_GLOBAL_GET_VALUE(FUNC_NAME);
  rocky_util_call_user_function_and_log_uncaught_error(func, jerry_create_undefined(), NULL, 0);
  jerry_release_value(func);
}

static void prv_do_eval(const char *eval_str) {
  rocky_util_eval_and_log_uncaught_error((const jerry_char_t *)eval_str, strlen(eval_str));
}

////////////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////////////

JERRY_FUNCTION(test_func_imp) {
  ++s_test_func_imp_call_count;
  return jerry_create_undefined();
}

JERRY_FUNCTION(method_func_imp) {
  ++s_method_func_imp_call_count;
  return jerry_create_undefined();
}

void test_rocky_api_util__rocky_add_constructor(void) {
  static const RockyGlobalAPI *s_api[] = {
    NULL,
  };
  rocky_global_init(s_api);

  JS_VAR prototype = rocky_add_constructor("test", test_func_imp);
  cl_assert_equal_b(jerry_value_is_object(prototype), true);
  EXECUTE_SCRIPT("_rocky.test();");
  cl_assert_equal_i(1, s_test_func_imp_call_count);

  rocky_add_function(prototype, "method", method_func_imp);
  EXECUTE_SCRIPT("var y = new _rocky.test(); y.method();");
  cl_assert_equal_i(1, s_method_func_imp_call_count);
}

void test_rocky_api_util__error_print(void) {
  s_log_internal__expected = (const char *[]){
    "Unhandled Error",
    "  "ERROR_STRING,
    NULL
  };

  jerry_value_t error_val =
      jerry_create_error(JERRY_ERROR_COMMON, (const jerry_char_t *)ERROR_STRING);
  cl_assert(jerry_value_has_error_flag(error_val));

  // NOTE: prv_log_uncaught_error() will call jerry_release_value(), so don't use error_val after
  // this call returns:
  prv_log_uncaught_error(error_val);

  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__call_no_error(void) {
  s_log_internal__expected = (const char *[]){ NULL };
  const char *script = "var "FUNC_NAME" = function() { return 1; };";
  prv_do_call_user_function(script);
  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__call_throw_string(void) {
  s_log_internal__expected = (const char *[]){
    "Unhandled exception",
    "  "ERROR_STRING,
    NULL
  };

  const char *script = "var "FUNC_NAME" = function() { throw '"ERROR_STRING"'; };";
  prv_do_call_user_function(script);
  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__call_throw_number(void) {
  s_log_internal__expected = (const char *[]){
    "Unhandled exception",
    "  1",
    NULL
  };
  const char *script = "var "FUNC_NAME" = function() { throw 1; };";
  prv_do_call_user_function(script);

  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__call_throw_error(void) {
  s_log_internal__expected = (const char *[]){
    "Unhandled Error",
    "  "ERROR_STRING,
    NULL
  };
  const char *script = "var "FUNC_NAME" = function() { throw new Error('"ERROR_STRING"'); };";
  prv_do_call_user_function(script);
  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__eval_no_error(void) {
  s_log_internal__expected = (const char *[]){ NULL };
  prv_do_eval("1+1;");
  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__eval_throw_string(void) {
  s_log_internal__expected = (const char *[]){
    "Unhandled exception",
    "  "ERROR_STRING,
    NULL
  };
  prv_do_eval("throw '"ERROR_STRING"';");
  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__eval_throw_error(void) {
  s_log_internal__expected = (const char *[]){
    "Unhandled Error",
    "  "ERROR_STRING,
    NULL
  };
  prv_do_eval("throw new Error('"ERROR_STRING"');");
  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_util__create_date_now(void) {
  const time_t cur_time = 1458250851;  // Thu Mar 17 21:40:51 2016 UTC
                                       // Thu Mar 17 14:40:51 2016 PDT
  const uint16_t cur_millis = 123;
  fake_time_init(cur_time, cur_millis);

  jerry_value_t now = rocky_util_create_date(NULL);
  jerry_value_t getSeconds = jerry_get_object_field(now, "getSeconds");
  jerry_value_t getMinutes = jerry_get_object_field(now, "getMinutes");
  jerry_value_t getHours = jerry_get_object_field(now, "getHours");
  jerry_value_t getDate = jerry_get_object_field(now, "getDate");

  jerry_value_t result_seconds = jerry_call_function(getSeconds, now, NULL, 0);
  jerry_value_t result_minutes = jerry_call_function(getMinutes, now, NULL, 0);
  jerry_value_t result_hours = jerry_call_function(getHours, now, NULL, 0);
  jerry_value_t result_date = jerry_call_function(getDate, now, NULL, 0);

  cl_assert(jerry_get_number_value(result_seconds) == 51.0);
  cl_assert(jerry_get_number_value(result_minutes) == 40.0);
  cl_assert(jerry_get_number_value(result_hours) == 21.0);
  cl_assert(jerry_get_number_value(result_date) == 17.0);

  jerry_release_value(result_date);
  jerry_release_value(result_hours);
  jerry_release_value(result_minutes);
  jerry_release_value(result_seconds);
  jerry_release_value(getDate);
  jerry_release_value(getHours);
  jerry_release_value(getMinutes);
  jerry_release_value(getSeconds);
  jerry_release_value(now);
}

void test_rocky_api_util__ecma_date_make_day(void) {
#ifdef EMSCRIPTEN
  printf("Skipping test %s", __FUNCTION__);
#else
  cl_assert_equal_d(16861, ecma_date_make_day(2016, 2, 1));  // JerryScript's unit-test
  cl_assert_equal_d(-25294, ecma_date_make_day(1900, 9, 1)); // not a leap year!
  cl_assert_equal_d(17075, ecma_date_make_day(2016, 8, 31)); // Sept-31 == Oct-01
  cl_assert_equal_d(17075, ecma_date_make_day(2016, 9, 1));  // Oct-01
  cl_assert_equal_d(17045, ecma_date_make_day(2016, 8, 1));  // Sept-01
#endif  // EMSCRIPTEN
}

void test_rocky_api_util__ecma_date_make_day_list(void) {
#ifdef EMSCRIPTEN
  printf("Skipping test %s", __FUNCTION__);
#else
  int fail_count = 0;
  for(int y = 1950; y < 2050; y++) {
    for(int m = 0; m < 12; m++) {
      for (int d = 1; d < 32; d++) {
        const ecma_number_t result = ecma_date_make_day(y, m, d);
        if (isnan(result)) {
          printf("failed for %04d-%02d-%02d\n", y, (m + 1), d);
          fail_count++;
        } else {
//          printf("passed for %04d-%02d-%02d: %d\n", y, (m + 1), d, (int)result);
        }
      }
    }
  }
  cl_assert_equal_i(0, fail_count);
#endif  // EMSCRIPTEN
}

void test_rocky_api_util__create_date_tm(void) {
  const time_t cur_time = 1458250851;  // Thu Mar 17 21:40:51 2016 UTC
                                       // Thu Mar 17 14:40:51 2016 PDT
  const uint16_t cur_millis = 123;
  fake_time_init(cur_time, cur_millis);
  struct tm tick_time = {
    .tm_sec = 28,
    .tm_min = 38,
    .tm_hour = 18,
    .tm_mday = 30,
    .tm_mon = 9,
    .tm_year = 116,
    .tm_wday = 1,
    .tm_yday = 275,
    .tm_zone = "\000\000\000\000\000",
  };

  jerry_value_t now = rocky_util_create_date(&tick_time);
  cl_assert(jerry_value_is_object(now));
  jerry_value_t getSeconds = jerry_get_object_field(now, "getSeconds");
  jerry_value_t getMinutes = jerry_get_object_field(now, "getMinutes");
  jerry_value_t getHours = jerry_get_object_field(now, "getHours");
  jerry_value_t getDate = jerry_get_object_field(now, "getDate");
  jerry_value_t getMonth = jerry_get_object_field(now, "getMonth");
  jerry_value_t getYear = jerry_get_object_field(now, "getYear");

  jerry_value_t result_seconds = jerry_call_function(getSeconds, now, NULL, 0);
  jerry_value_t result_minutes = jerry_call_function(getMinutes, now, NULL, 0);
  jerry_value_t result_hours = jerry_call_function(getHours, now, NULL, 0);
  jerry_value_t result_date = jerry_call_function(getDate, now, NULL, 0);
  jerry_value_t result_month = jerry_call_function(getMonth, now, NULL, 0);
  jerry_value_t result_year = jerry_call_function(getYear, now, NULL, 0);

  cl_assert_equal_d(jerry_get_number_value(result_seconds), 28.0);
  cl_assert_equal_d(jerry_get_number_value(result_minutes), 38.0);
  cl_assert_equal_d(jerry_get_number_value(result_hours), 18.0);
  cl_assert_equal_d(jerry_get_number_value(result_date), 30.0);
  cl_assert_equal_d(jerry_get_number_value(result_month), 9.0);
  cl_assert_equal_d(jerry_get_number_value(result_year), 116.0);

  jerry_release_value(result_year);
  jerry_release_value(result_month);
  jerry_release_value(result_date);
  jerry_release_value(result_hours);
  jerry_release_value(result_minutes);
  jerry_release_value(result_seconds);
  jerry_release_value(getYear);
  jerry_release_value(getMonth);
  jerry_release_value(getDate);
  jerry_release_value(getHours);
  jerry_release_value(getMinutes);
  jerry_release_value(getSeconds);
  jerry_release_value(now);
}
