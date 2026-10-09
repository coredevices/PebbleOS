/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <time.h>

#include <pbl/kernel/compiler.h>

#include <clar.h>

// Exercise the production module macros with this suite's explicit Kconfig symbols.
#undef UNITTEST
#include <pbl/logging/logging.h>
#define UNITTEST 1

#include <kernel/pebble_tasks.h>
#include <logging/logging_private.h>
#include <stubs_passert.h>

PBL_LOG_MODULE_DEFINE(test_runtime, CONFIG_TEST_RUNTIME_LOG_LEVEL);

extern bool g_pbl_log_enabled;
extern int g_pbl_log_level;
void test_log_runtime_peer(void);
void test_log_static_peer(int *evaluations);

static LogState s_state;
static int s_serial_count;
static int s_flash_count;
static uint8_t s_serial_level;
static uint8_t s_flash_level;
static bool s_serial_async;
static bool s_flash_async;
static bool s_irq_locked;
static bool s_sched_locked;
static char s_message[LOG_BUFFER_LENGTH];

PebbleTask pebble_task_get_current(void) {
  return PebbleTask_Unknown;
}

LogState *kernel_applib_get_log_state(void) {
  return &s_state;
}

void kernel_applib_release_log_state(LogState *state) {
}

LogState *app_state_get_log_state(void) {
  return NULL;
}

LogState *worker_state_get_log_state(void) {
  return NULL;
}

uint32_t sys_stack_free_bytes(void) {
  return 1024;
}

time_t sys_get_time(void) {
  return 0;
}

void sys_pbl_log(LogBinaryMessage *message, uint32_t flags) {
  kernel_pbl_log(message, flags);
}

bool pbl_irq_is_locked(void) {
  return s_irq_locked;
}

bool pbl_sched_is_locked(void) {
  return s_sched_locked;
}

void pulse_logging_log(uint8_t level, const char *filename, uint16_t line, const char *message) {
  s_serial_count++;
  s_serial_level = level;
  s_serial_async = true;
  snprintf(s_message, sizeof(s_message), "%s", message);
}

void pulse_logging_log_sync(uint8_t level, const char *filename, uint16_t line,
                            const char *message) {
  pulse_logging_log(level, filename, line, message);
  s_serial_async = false;
}

void pbl_log_advanced(const char *buffer, int length, bool async) {
  const LogBinaryMessage *message = (const LogBinaryMessage *)buffer;
  cl_assert_equal_i(length, sizeof(*message) + message->message_length);
  s_flash_count++;
  s_flash_level = message->log_level;
  s_flash_async = async;
}

void test_log_modules__initialize(void) {
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, CONFIG_TEST_RUNTIME_LOG_LEVEL);
  s_state = (LogState){};
  s_serial_count = s_flash_count = 0;
  s_serial_level = s_flash_level = 0;
  s_serial_async = s_flash_async = false;
  s_irq_locked = s_sched_locked = false;
  s_message[0] = '\0';
  g_pbl_log_enabled = true;
  g_pbl_log_level = LOG_LEVEL_INFO;
}

void test_log_modules__default_filters_debug_without_evaluating_arguments(void) {
  int evaluations = 0;
  PBL_LOG_DBG("debug %d", ++evaluations);
  cl_assert_equal_i(evaluations, 0);
  cl_assert_equal_i(s_serial_count, 0);
  cl_assert_equal_i(s_flash_count, 0);
  PBL_LOG_INFO("info");
  cl_assert_equal_i(s_serial_count, 1);
  cl_assert_equal_i(s_flash_count, 1);
  cl_assert_equal_s(s_message, "test_runtime: info");
}

void test_log_modules__debug_reaches_both_sinks_with_original_severity(void) {
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_DEBUG);
  PBL_LOG_DBG("debug");
  cl_assert_equal_i(s_serial_count, 1);
  cl_assert_equal_i(s_flash_count, 1);
  cl_assert_equal_i(s_serial_level, LOG_LEVEL_DEBUG);
  cl_assert_equal_i(s_flash_level, LOG_LEVEL_DEBUG);
  cl_assert(s_serial_async && s_flash_async);
  PBL_LOG_SYNC_DBG("sync debug");
  cl_assert_equal_i(s_serial_count, 2);
  cl_assert_equal_i(s_flash_count, 2);
  cl_assert(!s_serial_async && !s_flash_async);
}

void test_log_modules__level_is_shared_across_files_and_resets(void) {
  test_log_runtime_peer();
  cl_assert_equal_i(s_flash_count, 0);
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_DEBUG);
  test_log_runtime_peer();
  cl_assert_equal_i(s_flash_count, 1);
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, CONFIG_TEST_RUNTIME_LOG_LEVEL);
  test_log_runtime_peer();
  cl_assert_equal_i(s_flash_count, 1);
}

void test_log_modules__other_modules_keep_their_filters(void) {
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_DEBUG);
  int evaluations = 0;
  test_log_static_peer(&evaluations);
  cl_assert_equal_i(evaluations, 0);
  cl_assert_equal_i(s_flash_count, 1);
  // A debug message from another compiled-in source still obeys the sink levels.
  pbl_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, "other debug");
  cl_assert_equal_i(s_serial_count, 1);
  cl_assert_equal_i(s_flash_count, 1);
}

void test_log_modules__verbose_can_be_enabled_at_runtime(void) {
  int evaluations = 0;
  PBL_LOG_VERBOSE("verbose %d", ++evaluations);
  cl_assert_equal_i(evaluations, 0);
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_DEBUG_VERBOSE);
  cl_assert_equal_i(PBL_LOG_MODULE_LEVEL_GET(test_runtime), LOG_LEVEL_DEBUG_VERBOSE);
  PBL_LOG_VERBOSE("verbose %d", ++evaluations);
  cl_assert_equal_i(evaluations, 1);
  cl_assert_equal_i(s_serial_count, 1);
  cl_assert_equal_i(s_flash_count, 1);
  cl_assert_equal_i(s_serial_level, LOG_LEVEL_DEBUG_VERBOSE);
  cl_assert_equal_i(s_flash_level, LOG_LEVEL_DEBUG_VERBOSE);
}

void test_log_modules__off_and_warning_levels(void) {
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_WARNING);
  PBL_LOG_INFO("info");
  PBL_LOG_WRN("warning");
  cl_assert_equal_i(s_flash_count, 1);
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_OFF);
  PBL_LOG_ERR("error");
  PBL_LOG_ALWAYS("always");
  cl_assert_equal_i(s_flash_count, 1);
}

void test_log_modules__global_disable_and_unsafe_contexts_still_apply(void) {
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_DEBUG);
  g_pbl_log_enabled = false;
  PBL_LOG_DBG("disabled");
  cl_assert_equal_i(s_serial_count, 0);
  cl_assert_equal_i(s_flash_count, 0);
  g_pbl_log_enabled = true;
  s_irq_locked = true;
  PBL_LOG_DBG("irq locked");
  s_irq_locked = false;
  s_sched_locked = true;
  PBL_LOG_DBG("scheduler locked");
  cl_assert_equal_i(s_serial_count, 2);
  cl_assert_equal_i(s_flash_count, 0);
}
