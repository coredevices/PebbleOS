/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_tickservice.h"
#include "applib/rockyjs/pbl_jerry_port.h"

// Standard
#include "string.h"

// Fakes
#include "fake_app_timer.h"
#include "fake_logging.h"
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
#include "stubs_pbl_malloc.h"
#include "stubs_resources.h"
#include "stubs_sleep.h"
#include "stubs_serial.h"
#include "stubs_syscalls.h"
#include "stubs_sys_exit.h"

size_t heap_bytes_free(void) {
  return 123456;
}

void tick_timer_service_handle_time_change(void) {}

MockCallRecordings s_tick_timer_service_subscribe;
void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler) {
  record_mock_call(s_tick_timer_service_subscribe) {
    .tick_units = tick_units,
  };
}

void test_rocky_api_tickservice__initialize(void) {
  fake_app_timer_init();
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  s_tick_timer_service_subscribe = (MockCallRecordings){0};
  s_log_internal__expected = NULL;
}

void test_rocky_api_tickservice__cleanup(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
}

static const RockyGlobalAPI *s_api[] = {
  &TICKSERVICE_APIS,
  NULL,
};

void test_rocky_api_tickservice__provides_events(void) {
  rocky_global_init(s_api);

  cl_assert_equal_i(0, s_tick_timer_service_subscribe.call_count);
  cl_assert_equal_b(false, rocky_global_has_event_handlers("secondchange"));
  cl_assert_equal_b(false, rocky_global_has_event_handlers("minutechange"));
  cl_assert_equal_b(false, rocky_global_has_event_handlers("hourchange"));
  cl_assert_equal_b(false, rocky_global_has_event_handlers("daychange"));

  EXECUTE_SCRIPT("_rocky.on('daychange', function() {});");
  cl_assert_equal_b(false, rocky_global_has_event_handlers("secondchange"));
  cl_assert_equal_b(false, rocky_global_has_event_handlers("minutechange"));
  cl_assert_equal_b(false, rocky_global_has_event_handlers("hourchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("daychange"));
  cl_assert_equal_i(1, s_tick_timer_service_subscribe.call_count);
  cl_assert_equal_i(DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
                    s_tick_timer_service_subscribe.last_call.tick_units);

  EXECUTE_SCRIPT(
    "var hourHandler = function() {};\n"
    "_rocky.on('hourchange', hourHandler);\n"
  );
  cl_assert_equal_b(false, rocky_global_has_event_handlers("secondchange"));
  cl_assert_equal_b(false, rocky_global_has_event_handlers("minutechange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("hourchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("daychange"));
  cl_assert_equal_i(2, s_tick_timer_service_subscribe.call_count);
  cl_assert_equal_i(HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
                    s_tick_timer_service_subscribe.last_call.tick_units);

  EXECUTE_SCRIPT("_rocky.on('minutechange', function() {});");
  cl_assert_equal_b(false, rocky_global_has_event_handlers("secondchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("minutechange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("hourchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("daychange"));
  cl_assert_equal_i(3, s_tick_timer_service_subscribe.call_count);
  cl_assert_equal_i(MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
                    s_tick_timer_service_subscribe.last_call.tick_units);

  // register for minute again
  EXECUTE_SCRIPT("_rocky.on('minutechange', function() {});");
  cl_assert_equal_b(false, rocky_global_has_event_handlers("secondchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("minutechange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("hourchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("daychange"));
  cl_assert_equal_i(4, s_tick_timer_service_subscribe.call_count);
  cl_assert_equal_i(MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
                    s_tick_timer_service_subscribe.last_call.tick_units);


  EXECUTE_SCRIPT("_rocky.on('secondchange', function() {});");
  cl_assert_equal_b(true, rocky_global_has_event_handlers("secondchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("minutechange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("hourchange"));
  cl_assert_equal_b(true, rocky_global_has_event_handlers("daychange"));
  cl_assert_equal_i(5, s_tick_timer_service_subscribe.call_count);
  cl_assert_equal_i(SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
                    s_tick_timer_service_subscribe.last_call.tick_units);
}


void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed);

void test_rocky_api_tickservice__calls_handlers(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT(
    "var s = 0;\n"
    "var m = 0;\n"
    "var h = 0;\n"
    "var d = 0;\n"
    "_rocky.on('secondchange', function(e) {s++;});"
    "_rocky.on('minutechange', function(e) {m++;});"
    "_rocky.on('hourchange', function(e) {h++;});"
    "_rocky.on('daychange', function(e) {d++;});"
  );

  // subscribing already triggers a call
  ASSERT_JS_GLOBAL_EQUALS_I("s", 1);
  ASSERT_JS_GLOBAL_EQUALS_I("m", 1);
  ASSERT_JS_GLOBAL_EQUALS_I("h", 1);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);

  // all handlers will be called as year change means minute change
  prv_tick_handler(NULL, YEAR_UNIT);
  ASSERT_JS_GLOBAL_EQUALS_I("s", 2);
  ASSERT_JS_GLOBAL_EQUALS_I("m", 2);
  ASSERT_JS_GLOBAL_EQUALS_I("h", 2);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 2);

  // same here, each time a day changes, a second changes, too
  prv_tick_handler(NULL, MINUTE_UNIT | DAY_UNIT);
  ASSERT_JS_GLOBAL_EQUALS_I("s", 3);
  ASSERT_JS_GLOBAL_EQUALS_I("m", 3);
  ASSERT_JS_GLOBAL_EQUALS_I("h", 3);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 3);

  prv_tick_handler(NULL, HOUR_UNIT);
  ASSERT_JS_GLOBAL_EQUALS_I("s", 4);
  ASSERT_JS_GLOBAL_EQUALS_I("m", 4);
  ASSERT_JS_GLOBAL_EQUALS_I("h", 4);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 3);

  prv_tick_handler(NULL, MINUTE_UNIT);
  ASSERT_JS_GLOBAL_EQUALS_I("s", 5);
  ASSERT_JS_GLOBAL_EQUALS_I("m", 5);
  ASSERT_JS_GLOBAL_EQUALS_I("h", 4);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 3);

  prv_tick_handler(NULL, SECOND_UNIT);
  ASSERT_JS_GLOBAL_EQUALS_I("s", 6);
  ASSERT_JS_GLOBAL_EQUALS_I("m", 5);
  ASSERT_JS_GLOBAL_EQUALS_I("h", 4);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 3);
}

void test_rocky_api_tickservice__event_types(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT(
    "var s = null;\n"
    "var m = null;\n"
    "var h = null;\n"
    "var d = null;\n"
    "_rocky.on('secondchange', function(e) {s = e.type;});"
    "_rocky.on('minutechange', function(e) {m = e.type;});"
    "_rocky.on('hourchange', function(e) {h = e.type;});"
    "_rocky.on('daychange', function(e) {d = e.type;});"
  );

  // subscribing already triggers a call
  ASSERT_JS_GLOBAL_EQUALS_S("s", "secondchange");
  ASSERT_JS_GLOBAL_EQUALS_S("m", "minutechange");
  ASSERT_JS_GLOBAL_EQUALS_S("h", "hourchange");
  ASSERT_JS_GLOBAL_EQUALS_S("d", "daychange");
}

void test_rocky_api_tickservice__error_in_handler_on_register(void) {
  rocky_global_init(s_api);

  s_log_internal__expected = (const char *[]){
    "Unhandled exception",
    "  secondchange",
    NULL
  };
  EXECUTE_SCRIPT(
    "_rocky.on('secondchange', function(e) { throw e.type; });"
  );
  cl_assert(*s_log_internal__expected == NULL);
}

void test_rocky_api_tickservice__provides_event_date(void) {
  rocky_global_init(s_api);

  s_log_internal__expected = (const char *[]){ NULL };

  const time_t cur_time = 1458250851;  // Thu Mar 17 21:40:51 2016 UTC
                                       // Thu Mar 17 14:40:51 2016 PDT
  const uint16_t cur_millis = 123;
  fake_time_init(cur_time, cur_millis);

  EXECUTE_SCRIPT(
    "var s = null;\n"
    "var m = null;\n"
    "var h = null;\n"
    "var d = null;\n"
    "_rocky.on('secondchange', function(e) { s = e.date.getSeconds(); });\n"
    "_rocky.on('minutechange', function(e) { m = e.date.getMinutes(); });\n"
    "_rocky.on('hourchange',   function(e) { h = e.date.getHours();   });\n"
    "_rocky.on('daychange',    function(e) { d = e.date.getDate();    });\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_D("s", 51.0);
  ASSERT_JS_GLOBAL_EQUALS_D("m", 40.0);
  ASSERT_JS_GLOBAL_EQUALS_D("h", 21.0);
  ASSERT_JS_GLOBAL_EQUALS_D("d", 17.0);

  EXECUTE_SCRIPT(
    "s = null;\n"
    "m = null;\n"
    "h = null;\n"
    "d = null;\n"
  );

  struct tm tm = {
    .tm_sec = 1,
    .tm_min = 2,
    .tm_hour = 3,
    .tm_mday = 4,
    .tm_mon = 5,
    .tm_year = 116, // 2016
  };

  prv_tick_handler(&tm, SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT);
  ASSERT_JS_GLOBAL_EQUALS_D("s", 1.0);
  ASSERT_JS_GLOBAL_EQUALS_D("m", 2.0);
  ASSERT_JS_GLOBAL_EQUALS_D("h", 3.0);
  ASSERT_JS_GLOBAL_EQUALS_D("d", 4.0);

  cl_assert(*s_log_internal__expected == NULL);
}
