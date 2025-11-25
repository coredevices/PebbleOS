/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

typedef struct MainWindowData {
  Window *window;
  SimpleMenuLayer *menu_layer;
} MainWindowData;

static MainWindowData s_main_window_data;

static void execute_gibberish_menu_cb(int index, void *context) {
  int32_t gibberish[] = { 0, 0, 0, 0 };
  int8_t* gibberish_ptr = (int8_t*) gibberish;

  ((void (*)(void))gibberish_ptr + 1)();
}

static void write_to_null_menu_cb(int index, void *context) {
  int* null_ptr = NULL;
  *null_ptr = 0xdeadbeef;
}

static void write_to_kernel_menu_cb(int index, void *context) {
  // The kernel ram is between 0x20000000 to 0x20018000
  int* kernel_ptr = (int*) 0x20010000;
  *kernel_ptr = 0xdeadbeef;
}

static void trigger_applib_assert_cb(int index, void *context) {
  // A little fragile, I know we have an assert in this function but it may change in the future.
  layer_set_update_proc(0, 0);
}

static void trigger_infinite_loop(int index, void *context) {
  while (true) {
  }
}

static void trigger_persist_loop(int index, void *context) {
  int value = 1;
  while (true) {
    persist_write_int(42, value++);
  }
}

static void trigger_loop_log_spam(int index, void *context) {
  while (true) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Crash Demo Looping Log Spam! WarbleGarbleWarbleGarbleWarble");
  }
}


static void accel_data_handler(AccelData *data, uint32_t num_samples) {
}

static void trigger_to_app_event_flood(int index, void *context) {
  // Generate a crazy number of events and then busy wait.

  accel_data_service_subscribe(1, accel_data_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);

  while (true) {
  }
}

static void trigger_double_free(int index, void *context) {
  volatile int* storage = malloc(sizeof(int));
  *storage = 1337;
  free((void*) storage);
  free((void*) storage);
}

static void trigger_stack_overflow(int index, void *context) {
  volatile int counter = (int) context;
  if (counter > 300) {
    return;
  }
  ++counter;
  trigger_stack_overflow(index, (void*) counter);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  static const SimpleMenuItem menu_items[] = {
    {
      .title = "Execute gibberish",
      .callback = execute_gibberish_menu_cb
    }, {
      .title = "Write to NULL",
      .callback = write_to_null_menu_cb
    }, {
      .title = "Write to kernel",
      .callback = write_to_kernel_menu_cb
    }, {
      .title = "Trigger applib assert",
      .callback = trigger_applib_assert_cb
    }, {
      .title = "Infinite loop",
      .callback = trigger_infinite_loop
    }, {
      .title = "Loop Log Spam",
      .callback = trigger_loop_log_spam
    }, {
      .title = "To App Event Flood",
      .callback = trigger_to_app_event_flood
    }, {
      .title = "Double Free",
      .callback = trigger_double_free
    }, {
      .title = "Stack Overflow",
      .callback = trigger_stack_overflow
    }, {
      .title = "Persist loop",
      .callback = trigger_persist_loop
    }
  };
  static const SimpleMenuSection sections[] = {
    {
      .items = menu_items,
      .num_items = ARRAY_LENGTH(menu_items)
    }
  };

  s_main_window_data.menu_layer = simple_menu_layer_create(bounds, window, sections, ARRAY_LENGTH(sections), NULL);

  layer_add_child(window_layer, simple_menu_layer_get_layer(s_main_window_data.menu_layer));
}

static void window_unload(Window *window) {
  simple_menu_layer_destroy(s_main_window_data.menu_layer);
}

static void init(void) {
  Window *window = s_main_window_data.window;
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
}

int main(void) {
  init();

  app_event_loop();
  deinit();
}
