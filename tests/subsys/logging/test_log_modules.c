/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <pbl/kernel/compiler.h>

#include <clar.h>

// Exercise the production module macros with this suite's explicit Kconfig symbols.
#undef UNITTEST
#include <pbl/logging/logging.h>
#define UNITTEST 1

#include <pbl/logging/backend.h>

#include <logging/logging_private.h>

PBL_LOG_MODULE_DEFINE(test_runtime, CONFIG_TEST_RUNTIME_LOG_LEVEL);

extern bool g_pbl_log_enabled;
void test_log_runtime_peer(void);
void test_log_static_peer(int *evaluations);

static LogState s_state;
static int s_serial_count;
static int s_flash_count;
static uint8_t s_serial_level;
static uint8_t s_flash_level;
static bool s_serial_async;
static bool s_flash_async;
static char s_message[LOG_BUFFER_LENGTH];
static char s_flash_message[LOG_BUFFER_LENGTH];

LogState *pbl_log__state_get(void) {
  return &s_state;
}

void pbl_log__state_release(LogState *state) {
}

uint32_t pbl_log__stack_free(void) {
  return 1024;
}

uint32_t pbl_log__time(void) {
  return 0;
}

// Only the default hooks use these.
bool pbl_kernel_is_running(void) {
  return false;
}

size_t pbl_stack_free(void) {
  return 1024;
}

static void prv_serial_process(const struct pbl_log_backend *backend,
                               const struct pbl_log_msg *msg) {
  s_serial_count++;
  s_serial_level = msg->level;
  s_serial_async = msg->flags & PBL_LOG_MSG_ASYNC;
  snprintf(s_message, sizeof(s_message), "%s", msg->text);
}

static const struct pbl_log_backend_api s_serial_api = {
  .process = prv_serial_process,
};

PBL_LOG_BACKEND_DEFINE(test_serial, s_serial_api, LOG_LEVEL_INFO, true);

static void prv_flash_process(const struct pbl_log_backend *backend,
                              const struct pbl_log_msg *msg) {
  cl_assert_equal_i(msg->text_len, strlen(msg->text));
  s_flash_count++;
  s_flash_level = msg->level;
  s_flash_async = msg->flags & PBL_LOG_MSG_ASYNC;
  snprintf(s_flash_message, sizeof(s_flash_message), "%s", msg->text);
}

static const struct pbl_log_backend_api s_flash_api = {
  .process = prv_flash_process,
  .replay = true,
};

PBL_LOG_BACKEND_DEFINE(test_flash, s_flash_api, LOG_LEVEL_INFO, true);

void test_log_modules__initialize(void) {
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, CONFIG_TEST_RUNTIME_LOG_LEVEL);
  s_state = (LogState){};
  s_serial_count = s_flash_count = 0;
  s_serial_level = s_flash_level = 0;
  s_serial_async = s_flash_async = false;
  s_message[0] = '\0';
  s_flash_message[0] = '\0';
  g_pbl_log_enabled = true;
  pbl_log_backend_set_level(PBL_LOG_BACKEND_GET(test_serial), LOG_LEVEL_INFO);
  pbl_log_backend_set_level(PBL_LOG_BACKEND_GET(test_flash), LOG_LEVEL_INFO);
  pbl_log_backend_enable(PBL_LOG_BACKEND_GET(test_flash));
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

void test_log_modules__global_disable_applies(void) {
  PBL_LOG_MODULE_LEVEL_SET(test_runtime, LOG_LEVEL_DEBUG);
  g_pbl_log_enabled = false;
  PBL_LOG_DBG("disabled");
  cl_assert_equal_i(s_serial_count, 0);
  cl_assert_equal_i(s_flash_count, 0);
  g_pbl_log_enabled = true;
  PBL_LOG_DBG("enabled");
  cl_assert_equal_i(s_serial_count, 1);
  cl_assert_equal_i(s_flash_count, 1);
}

void test_log_modules__backend_levels_and_disable(void) {
  const struct pbl_log_backend *flash = PBL_LOG_BACKEND_GET(test_flash);
  pbl_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, "debug");
  cl_assert_equal_i(s_serial_count, 0);
  cl_assert_equal_i(s_flash_count, 0);
  pbl_log_backend_set_level(flash, LOG_LEVEL_DEBUG);
  pbl_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, "debug");
  cl_assert_equal_i(s_serial_count, 0);
  cl_assert_equal_i(s_flash_count, 1);
  pbl_log_backend_disable(flash);
  pbl_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "error");
  cl_assert_equal_i(s_serial_count, 1);
  cl_assert_equal_i(s_flash_count, 1);
  pbl_log_backend_enable(flash);
  pbl_log_backend_set_level(flash, LOG_LEVEL_INFO);
  cl_assert(pbl_log_backend_get_by_name("test_flash") == flash);
  cl_assert(pbl_log_backend_get_by_name("nope") == nullptr);
}

void test_log_modules__start_replays_messages_logged_while_disabled(void) {
  const struct pbl_log_backend *flash = PBL_LOG_BACKEND_GET(test_flash);
  pbl_log_backend_disable(flash);
  pbl_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "early");
  pbl_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, "not wanted");
  cl_assert_equal_i(s_flash_count, 0);

  pbl_log_backend_start(flash);
  cl_assert(pbl_log_backend_is_enabled(flash));
  cl_assert(s_flash_count >= 1);
  cl_assert_equal_s(s_flash_message, "early");
  cl_assert_equal_i(s_flash_level, LOG_LEVEL_ERROR);

  const int count = s_flash_count;
  pbl_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "live");
  cl_assert_equal_i(s_flash_count, count + 1);
  cl_assert_equal_s(s_flash_message, "live");
}

void test_log_modules__start_does_not_replay_without_replay(void) {
  const struct pbl_log_backend *serial = PBL_LOG_BACKEND_GET(test_serial);
  pbl_log_backend_disable(serial);
  pbl_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "not kept");

  pbl_log_backend_start(serial);
  cl_assert(pbl_log_backend_is_enabled(serial));
  cl_assert_equal_i(s_serial_count, 0);

  const struct pbl_log_backend *flash = PBL_LOG_BACKEND_GET(test_flash);
  pbl_log_backend_disable(flash);
  s_flash_message[0] = '\0';
  pbl_log_backend_start(flash);
  cl_assert(strcmp(s_flash_message, "not kept") != 0);
}
