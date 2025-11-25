/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_datetime.h"
#include "applib/rockyjs/pbl_jerry_port.h"

// Standard
#include <string.h>

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

static bool s_clock_is_24h_style;
bool clock_is_24h_style() {
  return s_clock_is_24h_style;
}


void test_rocky_api_datetime__initialize(void) {
  //  Mon Jul 25 2005 20:04:05 GMT-03:00
  s_time = 1122332645;
  s_gmt_off = -3 * 60 * 60;

  s_clock_is_24h_style = false;
  fake_app_timer_init();
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
}

void test_rocky_api_datetime__cleanup(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
}

static const RockyGlobalAPI *s_api[] = {
  &DATETIME_APIS,
  NULL,
};

void test_rocky_api_datetime__jerry_script_default(void) {
  static const RockyGlobalAPI *apis[] = {NULL};
  rocky_global_init(apis);
  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "var s1 = d.toString();\n"
    "var f = typeof(d.toLocaleTimeString);\n"
    "var s2 = d.toLocaleTimeString();\n"
    "var s3 = d.toLocaleDateString();\n"
    "var s4 = d.toLocaleString();\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_S("s1", "Mon Jul 25 2005 20:04:05 GMT-03:00");
  ASSERT_JS_GLOBAL_EQUALS_S("f", "function");
  // yes, JerryScript provides some default behavior but it's not what we want
  ASSERT_JS_GLOBAL_EQUALS_S("s2", "23:04:05.000");
  ASSERT_JS_GLOBAL_EQUALS_S("s3", "2005-07-25");
  ASSERT_JS_GLOBAL_EQUALS_S("s4", "Mon Jul 25 2005 20:04:05 GMT-03:00");
}

void test_rocky_api_datetime__locale_time_string_12h(void) {
  s_clock_is_24h_style = false;
  rocky_global_init(s_api);
  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "var s = d.toLocaleTimeString();\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_S("s", "8:04:05 PM");

  s_time += 4 * SECONDS_PER_HOUR;
  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "var s = d.toLocaleTimeString();\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_S("s", "12:04:05 AM");
}

void test_rocky_api_datetime__locale_time_string_24h(void) {
  s_clock_is_24h_style = true;
  rocky_global_init(s_api);
  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "var s = d.toLocaleTimeString();\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_S("s", "20:04:05");

  s_time += 4 * SECONDS_PER_HOUR;
  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "var s = d.toLocaleTimeString();\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_S("s", "00:04:05");
}

void test_rocky_api_datetime__locale(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "d.toLocaleTimeString(undefined);\n"
  );
  EXECUTE_SCRIPT_EXPECT_ERROR(
    "d.toLocaleTimeString('en-us');",
    "TypeError: Unsupported locale"
  );

  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "d.toLocaleDateString(undefined);\n"
  );
  EXECUTE_SCRIPT_EXPECT_ERROR(
    "d.toLocaleDateString('de');",
    "TypeError: Unsupported locale"
  );

  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "d.toLocaleString(undefined);\n"
  );
  EXECUTE_SCRIPT_EXPECT_ERROR(
    "d.toLocaleString('de');",
    "TypeError: Unsupported locale"
  );
}

void test_rocky_api_datetime__locale_time_string_options(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT("var d = new Date();");

  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {second: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "5");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {second: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "05");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {minute: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "4");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {minute: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "04");

  s_clock_is_24h_style = false;
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "8 PM");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, "
                 "{hour: 'numeric', hour12: true});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "8 PM");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, "
                 "{hour: 'numeric', hour12: false});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20");

  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "08 PM");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "8:04:05 PM");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, "
                         "{hour: undefined, minute: undefined, second: undefined});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "8:04:05 PM");


  s_clock_is_24h_style = true;
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, "
                   "{hour: 'numeric', hour12: true});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "8 PM");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, "
                   "{hour: 'numeric', hour12: false});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20:04:05");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, "
                   "{hour: undefined, minute: undefined, second: undefined});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20:04:05");

  EXECUTE_SCRIPT_EXPECT_ERROR(
    "d.toLocaleTimeString(undefined, {minute: 'numeric', hour: '2-digit'})",
    "TypeError: Unsupported options"
  );
}

void test_rocky_api_datetime__locale_time_string_date_options(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT("var d = new Date();");

  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {day: 'short'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "Mon, 8:04:05 PM");
}

void test_rocky_api_datetime__locale_date_string(void) {
  rocky_global_init(s_api);
  EXECUTE_SCRIPT(
    "var d = new Date();\n"
    "var s = d.toLocaleDateString();\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_S("s", "07/25/05");
}

void test_rocky_api_datetime__locale_date_string_options(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT("var d = new Date();");

  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "25");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "25");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: 'short'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "Mon");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: 'long'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "Monday");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "7");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "07");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: 'short'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "Jul");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: 'long'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "July");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {year: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "2005");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {year: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "05");
}

void test_rocky_api_datetime__locale_date_string_time_options(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT("var d = new Date();");

  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {hour: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "07/25/05, 8 PM");
}

void test_rocky_api_datetime__locale_string_options(void) {
  rocky_global_init(s_api);
  EXECUTE_SCRIPT("var d = new Date();");

  s_clock_is_24h_style = false;
  EXECUTE_SCRIPT("s = d.toLocaleString(undefined, {});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "07/25/05, 8:04:05 PM");
  s_clock_is_24h_style = true;
  EXECUTE_SCRIPT("s = d.toLocaleString(undefined, {});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "07/25/05, 20:04:05");

  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {second: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "5");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {second: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "05");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {minute: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "4");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {minute: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "04");

  s_clock_is_24h_style = false;
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "8 PM");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "08 PM");
  s_clock_is_24h_style = true;
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20");
  EXECUTE_SCRIPT("s = d.toLocaleTimeString(undefined, {hour: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "20");

  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "25");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "25");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: 'short'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "Mon");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {day: 'long'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "Monday");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "7");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "07");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: 'short'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "Jul");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {month: 'long'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "July");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {year: 'numeric'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "2005");
  EXECUTE_SCRIPT("s = d.toLocaleDateString(undefined, {year: '2-digit'});");
  ASSERT_JS_GLOBAL_EQUALS_S("s", "05");
}
