/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_console.h"

#include "applib/app_logging.h"
#include "rocky_api_util.h"
#include "system/passert.h"

#define ROCKY_CONSOLE "console"
#define ROCKY_CONSOLE_LOG "log"
#define ROCKY_CONSOLE_WARN "warn"
#define ROCKY_CONSOLE_ERROR "error"

static jerry_value_t prv_log(uint8_t level, jerry_length_t argc, const jerry_value_t argv[]) {
  for (size_t i = 0; i < argc; i++) {
    char buffer[100] = {0};
    jerry_object_to_string_to_utf8_char_buffer(argv[i], (jerry_char_t *)buffer, sizeof(buffer));
    app_log(level, "JS", 0, "%s", buffer);
  }
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_console_log) {
  return prv_log(APP_LOG_LEVEL_INFO, argc, argv);
}

JERRY_FUNCTION(prv_console_warn) {
  return prv_log(APP_LOG_LEVEL_WARNING, argc, argv);
}

JERRY_FUNCTION(prv_console_error) {
  return prv_log(APP_LOG_LEVEL_ERROR, argc, argv);
}

static void prv_init(void) {
  bool was_created;
  JS_VAR console =
    rocky_get_or_create_object(jerry_create_undefined(), ROCKY_CONSOLE, rocky_creator_object,
                               NULL, &was_created);

  // there must not be a global console object yet
  PBL_ASSERTN(was_created);

  rocky_add_function(console, ROCKY_CONSOLE_LOG, prv_console_log);
  rocky_add_function(console, ROCKY_CONSOLE_WARN, prv_console_warn);
  rocky_add_function(console, ROCKY_CONSOLE_ERROR, prv_console_error);
}

const RockyGlobalAPI CONSOLE_APIS = {
  .init = prv_init,
};
