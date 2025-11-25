/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_memory.h"
#include "applib/rockyjs/pbl_jerry_port.h"

#include "syscall/syscall.h"

#include <jmem/jmem-heap.h>

// Standard
#include <string.h>

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

size_t heap_bytes_free(void) {
  return 123456;
}

static int s_sys_analytics_inc_call_count;
void sys_analytics_inc(AnalyticsMetric metric, AnalyticsClient client) {
  cl_assert_equal_i(metric, ANALYTICS_APP_METRIC_MEM_ROCKY_RECURSIVE_MEMORYPRESSURE_EVENT_COUNT);
  ++s_sys_analytics_inc_call_count;
}

static const RockyGlobalAPI *s_api[] = {
  &MEMORY_APIS,
  NULL,
};

static bool s_skip_pbl_malloc_check;

#define assert_oom_app_fault() \
  cl_assert_equal_i(s_app_heap_analytics_log_rocky_heap_oom_fault_call_count, 1)

#define assert_no_oom_app_fault() \
  cl_assert_equal_i(s_app_heap_analytics_log_rocky_heap_oom_fault_call_count, 0)

void test_rocky_api_memory__initialize(void) {
  s_sys_analytics_inc_call_count = 0;
  s_app_heap_analytics_log_rocky_heap_oom_fault_call_count = 0;

  fake_pbl_malloc_clear_tracking();
  s_skip_pbl_malloc_check = false;

  s_log_internal__expected = NULL;

  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
  rocky_global_init(s_api);
}

void test_rocky_api_memory__cleanup(void) {
  rocky_global_deinit();
  jerry_cleanup();
  rocky_runtime_context_deinit();

  if (!s_skip_pbl_malloc_check) {
    fake_pbl_malloc_check_net_allocs();
  }
}

void test_rocky_api_memory__event(void) {
  cl_assert_equal_b(false, rocky_global_has_event_handlers("memorypressure"));

  jmem_heap_stats_t before_stats = {};
  jmem_heap_get_stats(&before_stats);

  EXECUTE_SCRIPT("_rocky.on('memorypressure', function(){});");

  // After registering a handler for 'memorypressure', expect a drop of more than N bytes of heap
  // because of the reservation of headroom space:
  jmem_heap_stats_t after_stats = {};
  jmem_heap_get_stats(&after_stats);
  cl_assert(after_stats.allocated_bytes - before_stats.allocated_bytes >=
            ROCKY_API_MEMORY_HEADROOM_DESIRED_SIZE_BYTES);

  cl_assert_equal_b(true, rocky_global_has_event_handlers("memorypressure"));
}

void test_rocky_api_memory__oom_app_fault_if_handler_allocates_more_than_headroom(void) {
  s_skip_pbl_malloc_check = true;
  cl_assert_passert(EXECUTE_SCRIPT(
    "var data = [];\n"
    "_rocky.on('memorypressure', function(){\n"
    "  var handlerData = [];\n"
    "  for (var i = 0; i < 100000; i++) {handlerData.push(i);}\n"
    "});\n"
    "for (var i = 0; i < 100000; i++) {data.push(i);}\n"
  ));
  assert_oom_app_fault();
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 1);
}

void test_rocky_api_memory__no_oom_app_fault_if_handler_frees_up_enough_memory_empty_array(void) {
  // Note to the reader: the lifecycle of `data` is not what you might think it is on first sight:
  // When `data = [];` executes, the original `data` will still be retained, because the original
  // execution context is still on the stack. Only after the  the 'memorypressure' handler returns
  // and that for(){} block finishes, is the original `data` released!

  EXECUTE_SCRIPT(
    "var data = [];\n"
    "var level = undefined;"
    "_rocky.on('memorypressure', function(e){\n"
    "  level = e.level;\n"
    "  data = [];\n"
    "});\n"
    "for (var i = 0; i < 100000; i++) {data.push(i);}\n"
  );
  assert_no_oom_app_fault();
  ASSERT_JS_GLOBAL_EQUALS_S("level", "high");
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 0);
}

void test_rocky_api_memory__no_oom_app_fault_if_handler_frees_up_enough_memory_empty_object(void) {
  EXECUTE_SCRIPT(
    "var data = {};\n"
    "_rocky.on('memorypressure', function(e){\n"
    "  data = {};\n"
    "});\n"
    "for (var i = 0; i < 100000; i++) {data[i] = i;}\n"
  );
  assert_no_oom_app_fault();
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 0);
}

void test_rocky_api_memory__no_oom_app_fault_if_handler_frees_up_enough_memory_put_props_for(void) {
  // This example uses a lot of properties on an Object to store things.
  // When running out of memory, these are dropped to free up memory, using the `delete` operator.
  EXECUTE_SCRIPT(
    "var first = 0;\n"
    "var i = 0;\n"
    "var obj = {};\n"
    "_rocky.on('memorypressure', function(e){\n"
    "  for (var j = first; j < i; j++) {\n"
    "    delete obj[j];\n"
    "  }\n"
    "  first = i;\n"
    "});\n"
    "for (i = first; i < 100000; i++) {\n"
    "  obj[i] = i;"
    "}\n"
  );
  assert_no_oom_app_fault();
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 0);
}

#if 0
// This doesn't work at the moment, because the `in` operator allocates a ton of memory... :( for
// the same reason as why Array.pop() has a footprint that's proportional to the number of elements.
void test_rocky_api_memory
__no_oom_app_fault_if_handler_frees_up_enough_mem_put_props_for_in(void) {
  EXECUTE_SCRIPT(
     "var obj = {};\n"
     "_rocky.on('memorypressure', function(e){\n"
     "  for (var p in obj) {\n"
     "    delete obj[p];\n"
     "  }\n"
     "});\n"
     "for (var i = 0; i < 100000; i++) {\n"
     "  obj['' + i] = i;"
     "}\n"
   );
  assert_no_oom_app_fault();
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 0);
}
#endif

#if 0
// This doesn't work because the putting the `length` property of an array end up calling
// ecma_op_object_get_property_names, which is has a memory footprint proportional to the number of
// elements/properties..
void test_rocky_api_memory
__no_oom_app_fault_if_handler_frees_up_enough_memory_put_length(void) {
  EXECUTE_SCRIPT(
    "var cache = [];\n"
    "_rocky.on('memorypressure', function(event) {\n"
    "  while (cache.length > 0) {\n"
    "    delete cache[cache.length - 1];\n"
    "    --cache.length;\n"
    "  }\n"
    "})\n;"
    "for (var i = 0; i < 100000; i++) {\n"
    "  cache.push(i);\n"
    "}\n"
  );
  assert_no_oom_app_fault();
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 0);
}
#endif

#if 0
// This doesn't work at the moment, because https://github.com/Samsung/jerryscript/issues/1370
void test_rocky_api_memory
__no_oom_app_fault_if_handler_frees_up_enough_memory_simple(void) {
  EXECUTE_SCRIPT(
    "var cache = [];\n"
    "_rocky.on('memorypressure', function(event) {\n"
    "  while (cache.length > 0) {\n"
    "    cache.pop();\n"
    "  }\n"
    "})\n;"
    "for (var i = 0; i < 100000; i++) {\n"
    "  cache.push(i);\n"
    "}\n"
  );
  assert_no_oom_app_fault();
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 0);
}
#endif

void test_rocky_api_memory__oom_app_fault_if_handler_does_not_free_up_enough_memory(void) {
  s_skip_pbl_malloc_check = true;

  s_log_internal__expected_regex = (const char *[]){
    "Memory pressure level: high",
    "heap size: [0-9]+, alloc'd: [0-9]+, waste: [0-9]+, largest free block: [0-9]+,",
    "used blocks: [0-9]+, free blocks: [0-9]+",
    "Fatal Error: 10",
    NULL };

  cl_assert_passert(EXECUTE_SCRIPT(
    "var data = [];\n"
    "var shouldContinue = true;\n"
    "_rocky.on('memorypressure', function(){\n"
    "  shouldContinue = false;\n"
    "});\n"
    "for (var i = 0; shouldContinue && i < 100000; i++) {data.push(i);}\n"
  ));
  assert_oom_app_fault();
  cl_assert_equal_i(s_sys_analytics_inc_call_count, 0);
  cl_assert(*s_log_internal__expected_regex == NULL);
}
