/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_util.h"
#include "rocky_api_errors.h"

#include "applib/app_logging.h"
#include "kernel/pbl_malloc.h"
#include "system/passert.h"
#include "util/size.h"
#include "util/trig.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ROCKY_SINGLETON "_rocky"

// [MT] including <math.h> causes the jerry-libm header to get included again :(
extern double round(double d);

uintptr_t rocky_util_uint_from_value(const jerry_value_t value) {
  uintptr_t rv = 0;
  if (jerry_value_is_number(value)) {
    rv = jerry_get_number_value(value);
  } else if (jerry_value_is_string(value)) {
    uint32_t sz = jerry_get_utf8_string_size(value);
    char buf[sz + 1];
    memset(buf, 0, sz + 1);
    jerry_string_to_utf8_char_buffer(value, (jerry_char_t *)buf, sz);
    rv = strtoul(buf, NULL, 0);
  }

  return rv;
}

int32_t jerry_get_int32_value(jerry_value_t value) {
  return (int32_t)(round(jerry_get_number_value(value)));
}

int32_t jerry_get_angle_value(jerry_value_t value) {
  return (int32_t)
      (jerry_get_number_value(value) * TRIG_MAX_ANGLE / (2 * M_PI)) + TRIG_MAX_ANGLE / 4;
}

char *rocky_string_alloc_and_copy(const jerry_value_t string) {
  char *out_str = NULL;

  if (jerry_value_is_string(string)) {
    uint32_t sz = jerry_get_utf8_string_size(string);
    out_str = task_zalloc_check(sz + 1);
    jerry_string_to_utf8_char_buffer(string, (jerry_char_t *)out_str, sz);
  }
  return out_str;
}

void rocky_log_exception(const char *message, jerry_value_t exception) {
  // using APP_LOG in this function so that 3rd-parties will know what went wrong with their JS
  APP_LOG(APP_LOG_LEVEL_ERROR, "Exception while %s", message);

  jerry_char_t buffer[100] = {};
  const ssize_t written = jerry_object_to_string_to_utf8_char_buffer(exception, buffer,
                                                                     sizeof(buffer) - 1);
  if (written > 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "%s", buffer);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "no further info.");
  }
}

void jerry_set_object_field(jerry_value_t object, const char *field, jerry_value_t value) {
  JS_VAR prop_name = jerry_create_string((jerry_char_t *)field);
  JS_UNUSED_VAL = jerry_set_property(object, prop_name, value);
}

jerry_value_t jerry_get_object_field(jerry_value_t object, const char* field) {
  JS_VAR prop_name = jerry_create_string((jerry_char_t *) field);
  const bool has_property = jerry_has_property(object, prop_name);
  JS_VAR value = (has_property) ? jerry_get_property(object, prop_name) : jerry_create_undefined();
  return jerry_acquire_value(value);
}

bool rocky_str_equal(jerry_value_t str_js, const char *str) {
  char buffer[40] = {0};
  PBL_ASSERTN(strlen(str) < sizeof(buffer));
  jerry_string_to_utf8_char_buffer(str_js, (jerry_char_t *) buffer, sizeof(buffer));
  return strcmp(buffer, str) == 0;
}

jerry_value_t jerry_get_object_getter_result(jerry_value_t object, const char *getter_name) {
  JS_VAR getter = jerry_get_object_field(object, getter_name);
  const bool is_function = jerry_value_is_function(getter);
  JS_VAR result = (is_function) ? jerry_call_function(getter, object, NULL, 0)
                                : jerry_create_undefined();
  return jerry_acquire_value(result);
}


jerry_value_t rocky_creator_object(void *ignore) {
  return jerry_create_object();
}

jerry_value_t rocky_creator_empty_array(void *ignore) {
  return jerry_create_array(0);
}

jerry_value_t rocky_get_or_create_object(jerry_value_t parent, const char *name,
                                         RockyObjectCreatorFunc creator_func, void *data,
                                         bool *was_created) {
  if (jerry_value_is_undefined(parent)) {
    parent = jerry_get_global_object();
    // it's safe to release the global object here already, it will never be destroyed
    jerry_release_value(parent);
  }

  // check the object doesn't already exist
  JS_VAR val = jerry_get_object_field(parent, name);
  if (!jerry_value_is_undefined(val)) {
    if (was_created) {
      *was_created = false;
    }
    return jerry_acquire_value(val);
  }

  JS_VAR result = creator_func(data);
  jerry_set_object_field(parent, name, result);

  if (was_created) {
    *was_created = true;
  }
  return jerry_acquire_value(result);
}

static jerry_value_t prv_create_function(void *c_function_ptr) {
  return jerry_create_external_function(c_function_ptr);
}

bool rocky_add_function(jerry_value_t parent, char *name, jerry_external_handler_t handler) {
  bool result = false;
  JS_UNUSED_VAL = rocky_get_or_create_object(parent, name, prv_create_function, handler, &result);
  return result;
}

bool rocky_add_global_function(char *name, jerry_external_handler_t handler) {
  return rocky_add_function(jerry_create_undefined(), name, handler);
}

jerry_value_t rocky_add_constructor(char *name, jerry_external_handler_t handler) {
  JS_VAR prototype = jerry_create_object();
  JS_VAR rocky_object = rocky_get_rocky_singleton();
  JS_VAR constructor = rocky_get_or_create_object(rocky_object, name,
                                                  prv_create_function, handler, NULL);
  // JerryScript doesn't create a prototype for external/C functions :( probably to save memory?
  jerry_set_object_field(prototype, "constructor", constructor);
  jerry_set_object_field(constructor, "prototype", prototype);
  return jerry_acquire_value(prototype);
}

jerry_value_t rocky_create_with_constructor(const char *rocky_constructor_name,
                                            const jerry_value_t args_p[],
                                            jerry_size_t args_count) {
  JS_VAR rocky_object = rocky_get_rocky_singleton();
  JS_VAR constructor = jerry_get_object_field(rocky_object, rocky_constructor_name);
  JS_VAR object = jerry_construct_object(constructor, args_p, args_count);

  return jerry_acquire_value(object);
}

// TODO: PBL-35780 make this part of app_state_get_rocky_runtime_context()
SECTION(".rocky_bss") static jerry_value_t s_rocky_singleton;
void rocky_set_rocky_singleton(jerry_value_t v) {
  s_rocky_singleton = jerry_acquire_value(v);
  JS_VAR global = jerry_get_global_object();
  jerry_set_object_field(global, ROCKY_SINGLETON, v);
}

jerry_value_t rocky_get_rocky_singleton(void) {
  return jerry_acquire_value(s_rocky_singleton);
}

void rocky_delete_singleton(void) {
  JS_VAR rocky_str = jerry_create_string((const jerry_char_t *)ROCKY_SINGLETON);
  JS_VAR global = jerry_get_global_object();
  jerry_delete_property(global, rocky_str);
}

void rocky_define_property(jerry_value_t parent, const char *prop_name,
                           jerry_external_handler_t getter,
                           jerry_external_handler_t setter) {
  jerry_property_descriptor_t prop_desc = {};
  jerry_init_property_descriptor_fields(&prop_desc);
  prop_desc.is_get_defined = getter != NULL;
  prop_desc.getter = jerry_create_external_function(getter);
  prop_desc.is_set_defined = setter != NULL;
  prop_desc.setter = jerry_create_external_function(setter);
  JS_VAR prop_name_js = jerry_create_string((const jerry_char_t *) prop_name);
  JS_UNUSED_VAL = jerry_define_own_property(parent, prop_name_js, &prop_desc);
  jerry_release_value(prop_desc.getter);
  jerry_release_value(prop_desc.setter);
}

// If result has an error flag set, log the error.
// Note: this function releases the passed in value.
T_STATIC void prv_log_uncaught_error(const jerry_value_t result) {
  if (jerry_value_has_error_flag(result)) {
    rocky_error_print(result);
  }
  jerry_release_value(result);
}

void rocky_util_eval_and_log_uncaught_error(const jerry_char_t *source_p, size_t source_size) {
  prv_log_uncaught_error(jerry_eval(source_p, source_size, false /*strict*/));
}

void rocky_util_call_user_function_and_log_uncaught_error(const jerry_value_t func_obj_val,
                                                          const jerry_value_t this_val,
                                                          const jerry_value_t args_p[],
                                                          jerry_size_t args_count) {
  prv_log_uncaught_error(jerry_call_function(func_obj_val, this_val, args_p, args_count));
}

jerry_value_t rocky_util_create_date(struct tm *tick_time) {
  JS_VAR date_constructor = jerry_get_global_builtin((const jerry_char_t *)"Date");
  if (!jerry_value_is_constructor(date_constructor)) {
    return jerry_create_undefined();
  }

  jerry_value_t date_obj;
  if (tick_time) {
    jerry_value_t args[] = {
        jerry_create_number(1900 + tick_time->tm_year), jerry_create_number(tick_time->tm_mon),
        jerry_create_number(tick_time->tm_mday), jerry_create_number(tick_time->tm_hour),
        jerry_create_number(tick_time->tm_min),  jerry_create_number(tick_time->tm_sec)
    };
    date_obj = jerry_construct_object(date_constructor, args, ARRAY_LENGTH(args));
    for (unsigned i = 0; i < ARRAY_LENGTH(args); ++i) {
      jerry_release_value(args[i]);
    }
  } else {
    date_obj = jerry_construct_object(date_constructor, NULL, 0);
  }
  return date_obj;
}

void rocky_cleanup_js_var(const jerry_value_t *var) {
  jerry_release_value(*var);
}
