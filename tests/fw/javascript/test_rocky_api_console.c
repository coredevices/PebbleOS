/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_console.h"
#include "applib/rockyjs/pbl_jerry_port.h"

// Standard
#include "string.h"

// Fakes
#include "fake_app_timer.h"
#include "fake_time.h"
#include "fake_logging.h"

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

static const RockyGlobalAPI *s_api[] = {
  &CONSOLE_APIS,
  NULL,
};

void test_rocky_api_console__initialize(void) {
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  s_log_internal__expected = NULL;
}

void test_rocky_api_console__cleanup(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
}

void test_rocky_api_console__functions_exist(void) {
  rocky_global_init(s_api);

  EXECUTE_SCRIPT(
    "var c = typeof console;\n"
    "var cl = typeof console.log;\n"
    "var cw = typeof console.warn;\n"
    "var ce = typeof console.error;\n"
  );

  ASSERT_JS_GLOBAL_EQUALS_S("c", "object");
  ASSERT_JS_GLOBAL_EQUALS_S("cl", "function");
  ASSERT_JS_GLOBAL_EQUALS_S("cw", "function");
  ASSERT_JS_GLOBAL_EQUALS_S("ce", "function");
}

void test_rocky_api_console__logs_single_values(void) {
  rocky_global_init(s_api);

  s_log_internal__expected = (const char *[]){
    "some string",
    "1234",
    "true",
    "undefined",
    "[object Object]",
    NULL
  };

  EXECUTE_SCRIPT(
    "console.log('some string');\n"
    "console.log(1230 + 4);\n"
    "console.log(1 == 1);\n"
    "console.log(undefined);\n"
    "console.log({a:123, b:[1,2]});\n"
  );

  cl_assert_equal_s(NULL, *s_log_internal__expected);
}

void test_rocky_api_console__warn_error_multiple(void) {
  rocky_global_init(s_api);

  s_log_internal__expected = (const char *[]){
    "foo",
    "1",
    "2",
    "true",
    "false",
    NULL
  };

  EXECUTE_SCRIPT(
    "console.warn('foo', 1, 2);\n"
    "console.error(true, false);\n"
  );

  cl_assert_equal_s(NULL, *s_log_internal__expected);
}