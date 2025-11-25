/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_timers.h"
#include "applib/rockyjs/pbl_jerry_port.h"

// Fakes
#include "fake_app_timer.h"
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


void test_rocky_api_timers__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  fake_app_timer_init();
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  TIMER_APIS.init();
}

void test_rocky_api_timers__cleanup(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
  fake_app_timer_deinit();
  fake_pbl_malloc_check_net_allocs();
}


void test_rocky_api_timers__setInterval(void) {
  char *script =
      "var num_times = 0;"
      "var extra_arg = 0;"
      "var timer = setInterval(function(extra) {"
      "num_times++;"
      "extra_arg = extra;"
      "}, 1000, 5);";

  EXECUTE_SCRIPT(script);

  AppTimer *timer = (AppTimer *)(uintptr_t)prv_js_global_get_double("timer");

  for (double d = 0.0; d < 5.0; d += 1.0) {
    ASSERT_JS_GLOBAL_EQUALS_I("num_times", d);
    cl_assert(fake_app_timer_is_scheduled(timer));
    cl_assert(app_timer_trigger(timer));
    ASSERT_JS_GLOBAL_EQUALS_I("extra_arg", 5.0);
  }

  script = "clearInterval(timer);";
  EXECUTE_SCRIPT(script);
  cl_assert(fake_app_timer_is_scheduled(timer) == false);
}

void test_rocky_api_timers__setTimeout(void) {
  char *script =
      "var num_times = 0;"
      "var f = function(extra) {"
      "  num_times++;"
      "};"
      "var timer = setTimeout('f()', '1000');";

  EXECUTE_SCRIPT(script);

  AppTimer *timer = (AppTimer *)(uintptr_t)prv_js_global_get_double("timer");
  cl_assert_equal_i(fake_app_timer_get_timeout(timer), 1000);
  cl_assert(fake_app_timer_is_scheduled(timer));
  cl_assert(app_timer_trigger(timer));

  ASSERT_JS_GLOBAL_EQUALS_I("num_times", 1.0);

  // Verified timer will not trigger again
  cl_assert(fake_app_timer_is_scheduled(timer) == false);
}

void test_rocky_api_timers__bogus_clearInterval(void) {
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearInterval(0)");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearInterval(1234)");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearInterval(-1234)");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearInterval(undefined)");
}

void test_rocky_api_timers__bogus_clearTimeout(void) {
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearTimeout(0)");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearTimeout(1234)");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearTimeout(-1234)");
  EXECUTE_SCRIPT_EXPECT_UNDEFINED("clearTimeout(undefined)");
}
