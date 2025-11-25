/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_global.h"

#include "applib/app_logging.h"
#include "jerry-api.h"
#include "kernel/pbl_malloc.h"
#include "system/passert.h"
#include "syscall/syscall.h"
#include "util/size.h"

#include "rocky_api.h"
#include "rocky_api_errors.h"
#include "rocky_api_util.h"

#define ROCKY_LISTENERS "_listeners"

#define ROCKY_ON "on"
#define ROCKY_ADD_EVENT_LISTENER "addEventListener"
#define ROCKY_OFF "off"
#define ROCKY_REMOVE_EVENT_LISTENER "removeEventListener"
#define ROCKY_EVENT_CONSTRUCTOR "Event"
#define ROCKY_EVENT_TYPE "type"

// TODO: PBL-35780 make this part of app_state_get_rocky_runtime_context()
SECTION(".rocky_bss") static const RockyGlobalAPI *const *s_global_apis;

#define API_REFS_FOREACH(var_name) \
  for (RockyGlobalAPI const *const *var_name = s_global_apis; *var_name != NULL; var_name++)


static jerry_value_t prv_get_or_create_listener_array(const char *event_name) {
  JS_VAR rocky = rocky_get_rocky_singleton();
  JS_VAR all_listeners = rocky_get_or_create_object(rocky, ROCKY_LISTENERS,
                                                           rocky_creator_object,
                                                           NULL, NULL);
  return rocky_get_or_create_object(all_listeners, event_name,
                                    rocky_creator_empty_array,
                                    NULL, NULL);
}

// callback while iterating over listeners
// @param event_listeners the array this listener is part of
// @param idx position within the array at which the listener exists
// @param listener the JS function that's registered as listener
// @param data pointer provided when calling prv_iterate_event_listeners()
// @return true, if you interested in further iterations. False to stop iterating.
typedef bool (*EventListenerIteratorCb)(jerry_value_t event_listeners, uint32_t idx,
                                        jerry_value_t listener, void *data);

static void prv_iterate_event_listeners(const char *event_name,
                                        EventListenerIteratorCb callback,
                                        void *data) {
  PBL_ASSERTN(callback);

  JS_VAR rocky = rocky_get_rocky_singleton();
  JS_VAR all_listeners = jerry_get_object_field(rocky, "_listeners");
  JS_VAR event_listeners = jerry_get_object_field(all_listeners, event_name);
//  printf("event_listeners.refcount: %d", jerry_port_)

  const uint32_t len = jerry_get_array_length(event_listeners);
  for (uint32_t idx = 0; idx < len; idx++) {
    JS_VAR listener = jerry_get_property_by_index(event_listeners, idx);
    if (jerry_value_is_function(listener)) {
      bool wants_more = callback(event_listeners, idx, listener, data);
      if (!wants_more) {
        break;
      }
    }
  }
}

typedef struct {
  jerry_value_t listener;
  bool found;
} ListenerQueryData;

static bool prv_find_listener(jerry_value_t event_listeners, uint32_t idx,
                       jerry_value_t listener, void *data) {
  ListenerQueryData *query_data = data;
  if (query_data->listener == listener) {
    query_data->found = true;
    return false;
  }
  return true;
}

static bool prv_listener_is_registered(const char *event_name, jerry_value_t listener) {
  ListenerQueryData data = {
    .listener = listener,
  };
  prv_iterate_event_listeners(event_name, prv_find_listener, &data);
  return data.found;
}

T_STATIC void prv_add_event_listener_to_list(const char *event_name, jerry_value_t listener) {
  if (prv_listener_is_registered(event_name, listener)) {
    // we won't register the same listener twice
    return;
  }
  JS_VAR listeners = prv_get_or_create_listener_array(event_name);
  const uint32_t num_entries = jerry_get_array_length(listeners);
  JS_UNUSED_VAL = jerry_set_property_by_index(listeners, num_entries, listener);
}

static bool prv_remove_listener(jerry_value_t event_listeners, uint32_t idx,
                                jerry_value_t listener, void *data) {
  ListenerQueryData *query_data = data;
  if (query_data->listener == listener) {
    // calling `event_listeners.splice(idx, 1)` to remove item at idx
    // wow, this is a lot of code for this
    JS_VAR splice = jerry_get_object_field(event_listeners, "splice");
    const jerry_value_t args[] = {jerry_create_number(idx), jerry_create_number(1)};
    JS_VAR remove_result = jerry_call_function(splice, event_listeners,
                                               args, ARRAY_LENGTH(args));
    if (jerry_value_has_error_flag(remove_result)) {
      rocky_log_exception("removing event listener", remove_result);
    }
    for (size_t i = 0; i < ARRAY_LENGTH(args); i++) {
      jerry_release_value(args[i]);
    }

    // mark item as removed and stop iterating
    query_data->found = true;
    return false;
  }
  return true;
}

T_STATIC bool prv_remove_event_listener_from_list(const char *event_name, jerry_value_t listener) {
  ListenerQueryData data = {
    .listener = listener,
  };
  prv_iterate_event_listeners(event_name, prv_remove_listener, &data);
  return data.found;
}


// implementation of .on(event_name, handler) and stores them in a new property
//
//  rocky._listeners = {
//    "event_1" : [ function_1, function_2, ... ],
//    "event_2" ; [ ... ] ,
//    ...
//  }
//
// please note that we ignore events, no API is interested in by asking each of them
// via .add_handler(event_name, func)

static jerry_value_t prv_event_listener_extract_args(const jerry_length_t argc,
                                                     const jerry_value_t argv[],
                                                     char *event_name, size_t event_name_size,
                                                     jerry_value_t *func) {
  PBL_ASSERTN(event_name && func);

  if (argc < 2) {
    return rocky_error_arguments_missing();
  }

  memset(event_name, 0, event_name_size);
  const jerry_size_t len = jerry_string_to_utf8_char_buffer(argv[0],
                                                            (jerry_char_t *)event_name,
                                                            event_name_size);
  if (len == 0 || len == event_name_size) {
    return rocky_error_argument_invalid("Not a valid event");
  }

  if (!jerry_value_is_function(argv[1])) {
    return rocky_error_argument_invalid("Not a valid handler");
  }
  *func = argv[1];

  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_add_event_listener) {
  char event_name[32];
  jerry_value_t func; // out parameter &func will not be acquired
  JS_VAR arg_result = prv_event_listener_extract_args(argc, argv,
                                                      event_name, sizeof(event_name),
                                                      &func);
  if (jerry_value_has_error_flag(arg_result)) {
    return jerry_acquire_value(arg_result);
  }

  bool is_relevant = false;
  API_REFS_FOREACH(api_ref) {
    if ((*api_ref)->add_handler) {
      is_relevant = (*api_ref)->add_handler(event_name, func);
      if (is_relevant) {
        break;
      }
    }
  }

  if (!is_relevant) {
    APP_LOG(LOG_LEVEL_WARNING, "Unknown event '%s'", event_name);
    return jerry_create_undefined();
  }

  prv_add_event_listener_to_list((char *)event_name, func);

  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_remove_event_listener) {
  char event_name[32];
  jerry_value_t func; // out parameter &func will not be acquired
  JS_VAR arg_result = prv_event_listener_extract_args(argc, argv,
                                                      event_name, sizeof(event_name),
                                                      &func);
  if (jerry_value_has_error_flag(arg_result)) {
    return jerry_acquire_value(arg_result);
  }

  const bool removed = prv_remove_event_listener_from_list(event_name, func);
  if (removed) {
    API_REFS_FOREACH(api_ref) {
      if ((*api_ref)->remove_handler) {
        (*api_ref)->remove_handler(event_name, func);
      }
    }
  } else {
    APP_LOG(LOG_LEVEL_WARNING, "Unknown handler for event '%s'", event_name);
  }

  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_event_constructor) {
  if (argc < 1) {
    return rocky_error_arguments_missing();
  }
  if (!jerry_value_is_string(argv[0])) {
    return rocky_error_unexpected_type(0, "String");
  }
  jerry_set_object_field(this_val, ROCKY_EVENT_TYPE, argv[0]);
  return jerry_create_undefined();
}

static void prv_copy_property(const jerry_value_t rocky,
                              const char *name_from, const char *name_to) {
  JS_VAR on = jerry_get_object_field(rocky, name_from);
  jerry_set_object_field(rocky, name_to, on);
}

void rocky_global_init(const RockyGlobalAPI *const *global_apis) {
  PBL_ASSERTN(global_apis);
  s_global_apis = global_apis;

  JS_VAR rocky = jerry_create_object();
  // this keeps a permanent reference to the singleton
  rocky_set_rocky_singleton(rocky);

  rocky_add_function(rocky, ROCKY_ON, prv_add_event_listener);
  prv_copy_property(rocky, ROCKY_ON, ROCKY_ADD_EVENT_LISTENER);
  rocky_add_function(rocky, ROCKY_OFF, prv_remove_event_listener);
  prv_copy_property(rocky, ROCKY_OFF, ROCKY_REMOVE_EVENT_LISTENER);

  JS_UNUSED_VAL = rocky_add_constructor(ROCKY_EVENT_CONSTRUCTOR, prv_event_constructor);

  API_REFS_FOREACH(api_ref) {
    if ((*api_ref)->init) {
      (*api_ref)->init();
    }
  }
}

void rocky_global_deinit(void) {
  API_REFS_FOREACH(api_ref) {
    if ((*api_ref)->deinit) {
      (*api_ref)->deinit();
    }
  }

#if APPLIB_EMSCRIPTEN
  rocky_delete_singleton();
#endif
}

static bool prv_has_any(jerry_value_t event_listeners, uint32_t idx,
                        jerry_value_t listener, void *data) {
  *(bool *)data = true;
  return false;
}

bool rocky_global_has_event_handlers(const char *event_name) {
  bool result = false;
  prv_iterate_event_listeners(event_name, prv_has_any, &result);
  return result;
}

typedef struct {
  const jerry_value_t this_arg;
  const jerry_value_t *args_p;
  jerry_size_t args_count;
} EventHandlersCallArgs;

static bool prv_call_event_handlers_cb(jerry_value_t event_listeners, uint32_t idx,
                                       jerry_value_t listener, void *data) {
  EventHandlersCallArgs * const args = data;
  rocky_util_call_user_function_and_log_uncaught_error(listener, args->this_arg, args->args_p,
                                                       args->args_count);
  return true;
}

void rocky_global_call_event_handlers(jerry_value_t event) {
  EventHandlersCallArgs args = {
    .this_arg =  jerry_create_undefined(),
    .args_p = &event,
    .args_count = 1,
  };

  JS_VAR event_str = jerry_get_object_field(event, "type");
  char *event_name = rocky_string_alloc_and_copy(event_str);
  prv_iterate_event_listeners(event_name, prv_call_event_handlers_cb, &args);

  task_free(event_name);
  jerry_release_value(args.this_arg);
}

static void prv_call_event_handlers_async_cb(void *ctx) {
  jerry_value_t event = (jerry_value_t)(uintptr_t) ctx;
  rocky_global_call_event_handlers(event);
  jerry_release_value(event);  // was acquired in rocky_global_call_event_handlers_async() call
}

void rocky_global_call_event_handlers_async(jerry_value_t event) {
  sys_current_process_schedule_callback(prv_call_event_handlers_async_cb,
                                        (void *)(uintptr_t)jerry_acquire_value(event));
}

jerry_value_t rocky_global_create_event(const char *type_str) {
  JS_VAR jerry_type_str = jerry_create_string_utf8((const jerry_char_t *)type_str);
  JS_VAR event = rocky_create_with_constructor(ROCKY_EVENT_CONSTRUCTOR, &jerry_type_str, 1);
  return jerry_acquire_value(event);
}
