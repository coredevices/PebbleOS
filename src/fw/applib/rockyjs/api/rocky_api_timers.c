/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_errors.h"
#include "jerry-api.h"
#include "rocky_api_util.h"
#include "applib/app_timer.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "rocky_api.h"

#define ROCKY_SETINTERVAL "setInterval"
#define ROCKY_SETTIMEOUT "setTimeout"
#define ROCKY_CLEARTIMEOUT "clearTimeout"
#define ROCKY_CLEARINTERVAL "clearInterval"

typedef struct {
  bool is_repeating;
  jerry_value_t callback;
  AppTimer *timer;
  jerry_length_t argc;
  jerry_value_t argv[];
} RockyTimerCbData;

static void prv_timer_cleanup(RockyTimerCbData *timer_data) {
  jerry_release_value(timer_data->callback);
  for (unsigned i = 0; i < timer_data->argc; i++) {
    jerry_release_value(timer_data->argv[i]);
  }
  task_free(timer_data);
}

static void prv_timer_callback(void *data) {
  RockyTimerCbData *timer_data = data;

  if (jerry_value_is_function(timer_data->callback)) {
    rocky_util_call_user_function_and_log_uncaught_error(
        timer_data->callback, jerry_create_undefined(), timer_data->argv, timer_data->argc);
  } else if (jerry_value_is_string(timer_data->callback)) {
    char *source_buf = rocky_string_alloc_and_copy(timer_data->callback);
    rocky_util_eval_and_log_uncaught_error((const jerry_char_t *)source_buf, strlen(source_buf));
    task_free(source_buf);
  }

  if (!timer_data->is_repeating) {
    prv_timer_cleanup(timer_data);
  }
}

static jerry_value_t prv_create_timer(const jerry_value_t *argv,
                                      const jerry_length_t argc,
                                      bool is_repeating) {
  if (argc < 1) {
    return rocky_error_arguments_missing();
  }

  jerry_value_t callback = argv[0];
  if (!jerry_value_is_function(callback) &&
      !jerry_value_is_string(callback)) {
    // Nothing to call, but somehow this is valid ¯\_(ツ)_/¯, no-op
    return jerry_create_number(0);
  }
  jerry_acquire_value(callback);

  uint32_t timeout = 0;
  jerry_length_t cb_argc = 0;
  if (argc >= 2) {
    // both numbers (123) and strings ('123') are valid
    // all others are 0
    timeout = rocky_util_uint_from_value(argv[1]);
    cb_argc = argc - 2;
  }

  RockyTimerCbData *cb_data = task_zalloc_check(sizeof(RockyTimerCbData) +
                                                cb_argc * sizeof(jerry_value_t));
  *cb_data = (RockyTimerCbData){
    .is_repeating = is_repeating,
    .callback = callback,
    .argc = cb_argc
  };
  // copy arguments over to cb_data
  for (unsigned i = 0; i < cb_argc; i++) {
    cb_data->argv[i] = argv[i + 2];
    jerry_acquire_value(cb_data->argv[i]);
  }
  cb_data->timer = app_timer_register_repeatable(timeout,
                                                 prv_timer_callback,
                                                 cb_data,
                                                 is_repeating);
  return jerry_create_number((uintptr_t) cb_data->timer);
}

JERRY_FUNCTION(setInterval_handler) {
  return prv_create_timer(argv, argc, true /*is_repeating*/);
}

JERRY_FUNCTION(setTimeout_handler) {
  return prv_create_timer(argv, argc, false /*is_repeating*/);
}

JERRY_FUNCTION(clearTimer_handler) {
  if (argc < 1 || !jerry_value_is_number(argv[0])) {
    // Somehow this is valid ¯\_(ツ)_/¯, no-op
    return jerry_create_undefined();
  }

  AppTimer *timer =
      (AppTimer *)(uintptr_t)rocky_util_uint_from_value(argv[0]);
  RockyTimerCbData *timer_data = app_timer_get_data(timer);
  app_timer_cancel(timer);
  if (timer_data) {
    prv_timer_cleanup(timer_data);
  }

  return jerry_create_undefined();
}

static void prv_init_apis(void) {
  rocky_add_global_function(ROCKY_SETINTERVAL, setInterval_handler);
  rocky_add_global_function(ROCKY_SETTIMEOUT, setTimeout_handler);
  rocky_add_global_function(ROCKY_CLEARTIMEOUT, clearTimer_handler);
  rocky_add_global_function(ROCKY_CLEARINTERVAL, clearTimer_handler);
}

const RockyGlobalAPI TIMER_APIS = {
  .init = prv_init_apis,
};
