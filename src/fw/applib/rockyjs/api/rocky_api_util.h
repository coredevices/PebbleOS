/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "jerry-api.h"
#include "util/macro.h"
#include "util/time/time.h"

#include <stdlib.h>


// Create a const jerry value that will be released when it goes out of scope
#define JS_VAR const jerry_value_t __attribute__((cleanup(rocky_cleanup_js_var)))

// Create a temporary jerry value with a unique name that will be released when it goes out of scope
#define JS_UNUSED_VAL \
    const jerry_value_t __attribute__((unused,cleanup(rocky_cleanup_js_var))) MACRO_CONCAT(js, __COUNTER__)

void rocky_cleanup_js_var(const jerry_value_t *var);

uintptr_t rocky_util_uint_from_value(const jerry_value_t value);

//! Note: you need to free the return value explicitly
char *rocky_string_alloc_and_copy(const jerry_value_t string);

void rocky_log_exception(const char *message, jerry_value_t exception);

#define ROCKY_RETURN_IF_ERROR(expr) \
  do { \
    const jerry_value_t rv = (expr); \
    if (jerry_value_has_error_flag(rv)) { \
      return rv; \
    } \
  } while (0)

#define JERRY_FUNCTION(name) static jerry_value_t name(const jerry_value_t function_obj_p, \
                                                       const jerry_value_t this_val, \
                                                       const jerry_value_t argv[], \
                                                       const jerry_length_t argc)

void jerry_set_object_field(jerry_value_t object, const char *field, jerry_value_t value);
jerry_value_t jerry_get_object_field(jerry_value_t object, const char *field);
jerry_value_t jerry_get_object_getter_result(jerry_value_t object, const char *getter_name);

bool rocky_add_function(jerry_value_t parent, char *name, jerry_external_handler_t handler);
bool rocky_add_global_function(char *name, jerry_external_handler_t handler);

//! Adds a constructor function object to rocky.name (it sets up the prototype of the function
//! which JerryScript normally does not do for external functions).
//! @return the prototype object
jerry_value_t rocky_add_constructor(char *name, jerry_external_handler_t handler);

// Creates an object using a global constructor, in other words: `new constructor_name(args)`
jerry_value_t rocky_create_with_constructor(const char *rocky_constructor_name,
                                            const jerry_value_t args_p[],
                                            jerry_size_t args_count);

// does rounding to avoid Math.sin(2*Math.PI) issues and related problems
int32_t jerry_get_int32_value(jerry_value_t value);

// converts JS angle (0 degrees at 3 o'clock, 360 degrees = 2 * PI)
// to Pebble angle (0 degrees at 12 o'clock, 360 degrees = TRIG_MAX_ANGLE)
int32_t jerry_get_angle_value(jerry_value_t value);

typedef jerry_value_t (*RockyObjectCreatorFunc)(void *data);

// implementations of RockyObjectCreatorFunc for convenience of rocky_get_or_create_object
// these functions simply ignore the parameter
jerry_value_t rocky_creator_object(void *ignore);
jerry_value_t rocky_creator_empty_array(void *ignore);

jerry_value_t rocky_get_or_create_object(jerry_value_t parent, const char *name,
                                         RockyObjectCreatorFunc creator_func, void *data,
                                         bool *was_created);

// True, if jerry value represents a string that is equal to a given char buffer
bool rocky_str_equal(jerry_value_t str_js, const char *str);

void rocky_set_rocky_singleton(jerry_value_t v);

// caller needs to call jerry_release_value() on return value
jerry_value_t rocky_get_rocky_singleton(void);

void rocky_delete_singleton(void);

void rocky_define_property(jerry_value_t parent, const char *prop_name,
                           jerry_external_handler_t getter,
                           jerry_external_handler_t setter);

void rocky_util_eval_and_log_uncaught_error(const jerry_char_t *source_p, size_t source_size);

void rocky_util_call_user_function_and_log_uncaught_error(const jerry_value_t func_obj_val,
                                                          const jerry_value_t this_val,
                                                          const jerry_value_t args_p[],
                                                          jerry_size_t args_count);

jerry_value_t rocky_util_create_date(struct tm *tick_time);
