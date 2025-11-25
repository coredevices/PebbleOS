/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_errors.h"
#include "rocky_api_util.h"

#include "applib/app_logging.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "system/passert.h"

#include <inttypes.h>
#include <stdint.h>

jerry_value_t rocky_error_arguments_missing(void) {
  jerry_char_t *msg = (jerry_char_t *)"Not enough arguments";
  return jerry_create_error(JERRY_ERROR_TYPE, msg);
}

jerry_value_t rocky_error_argument_invalid(const char *msg) {
  return jerry_create_error(JERRY_ERROR_TYPE, (jerry_char_t *)msg);
}

jerry_value_t rocky_error_argument_invalid_at_index(uint32_t arg_idx, const char *error_msg) {
  char buffer[100] = {0};
  snprintf(buffer, sizeof(buffer), "Argument at index %"PRIu32" is invalid: %s",
           arg_idx, error_msg);
  return rocky_error_argument_invalid(buffer);
}

jerry_value_t rocky_error_unexpected_type(uint32_t arg_idx, const char *expected_type_name) {
  char buffer[100] = {0};
  snprintf(buffer, sizeof(buffer), "Argument at index %"PRIu32" is not a %s",
           arg_idx, expected_type_name);
  return rocky_error_argument_invalid(buffer);
}

static jerry_value_t prv_error_two_parts(jerry_error_t error_type,
                                         const char *left, const char *right) {
  char buffer[100] = {0};
  snprintf(buffer, sizeof(buffer), "%s%s", left, right);
  return jerry_create_error(error_type, (jerry_char_t *)buffer);
}

jerry_value_t rocky_error_oom(const char *hint) {
  return prv_error_two_parts(JERRY_ERROR_RANGE, "Out of memory: ", hint);
}

static char * prv_get_string_from_field(jerry_value_t object, const jerry_char_t *property) {
  JS_VAR prop_name = jerry_create_string((const jerry_char_t *) property);
  JS_VAR prop_val = jerry_get_property(object, prop_name);
  JS_VAR prop_str = jerry_value_to_string(prop_val);
  char *result = rocky_string_alloc_and_copy(prop_str);
  return result;
}

// from lit-magic-string.inc.h
const jerry_char_t ERROR_NAME_PROPERTY_NAME[] = "name";
const jerry_char_t ERROR_MSG_PROPERTY_NAME[] = "message";

void rocky_error_print(jerry_value_t error_val) {
  char *name = NULL;
  char *msg = NULL;
  if (jerry_value_is_object(error_val)) {
    name = prv_get_string_from_field(error_val, ERROR_NAME_PROPERTY_NAME);
    msg = prv_get_string_from_field(error_val, ERROR_MSG_PROPERTY_NAME);
  } else {
    jerry_value_clear_error_flag(&error_val);
    JS_VAR error_str = jerry_value_to_string(error_val);
    msg = rocky_string_alloc_and_copy(error_str);
  }

  if (name) {
    APP_LOG(LOG_LEVEL_ERROR, "Unhandled %s", name);
  } else {
    APP_LOG(LOG_LEVEL_ERROR, "Unhandled exception");
  }

  if (msg) {
    APP_LOG(LOG_LEVEL_ERROR, "  %s", msg);
  }

  task_free(name);
  task_free(msg);
}
