/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "process_management/pebble_process_md.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "services/common/comm_session/session.h"
#include "services/common/comm_session/session_send_buffer.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "system/passert.h"

#define NUM_MENU_ITEMS 2

// =================================================================================
// Application Data
typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
  SimpleMenuItem menu_items[NUM_MENU_ITEMS];
  
} TestBTAppData;

static TestBTAppData *s_app_data = 0;


static volatile int s_pending_count = 0;
static volatile bool s_connected = false;

// =================================================================================
static void send_bluetooth(void* data) {
  //uint8_t *buffer = (uint8_t *)"hello world";

  CommSession *session = comm_session_get_system_session();
  if (!session) {
    s_pending_count--;
    s_connected = false;
    return;
  }

  PBL_LOG(LOG_LEVEL_INFO, "sending data");
  comm_session_send_data(session, 2000, (uint8_t *)0x08000000,
                         comm_session_send_buffer_get_max_payload_length(session),
                         COMM_SESSION_DEFAULT_TIMEOUT);
  s_pending_count--;
}


// =================================================================================
// You can capture when the user selects a menu icon with a menu item select callback
static void menu_select_callback(int index, void *ctx) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Hit menu item %d", index);
  
  // Here we just change the subtitle to a literal string
  s_app_data->menu_items[index].subtitle = "You've hit select here!";
  
  // Mark the layer to be updated
  layer_mark_dirty(simple_menu_layer_get_layer(s_app_data->menu_layer));
  
  // ---------------------------------------------------------------------------
  // Run the appropriate test
  if (index == 0) {
    s_connected = true;

    // Flood bluetooth
    while (s_connected) {
      while (s_pending_count > 6 && s_connected) {
        psleep(100);
      }
      s_pending_count++;
      system_task_add_callback(send_bluetooth, NULL);
    }
    PBL_LOG(LOG_LEVEL_INFO, "Bluetooth disconnected");

  } else if (index == 1) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Not implemented");

  }
}


// =================================================================================
static void prv_window_load(Window *window) {
  
  TestBTAppData *data = s_app_data;
  
  int i = 0;
  data->menu_items[i++] = (SimpleMenuItem) {
    .title = "flood BT",
    .callback = menu_select_callback,
  };
  data->menu_items[i++] = (SimpleMenuItem) {
    .title = "Ad space available",
    .callback = menu_select_callback,
  };
  PBL_ASSERTN(i == NUM_MENU_ITEMS);
  
  // The menu sections
  data->menu_section = (SimpleMenuSection) {
    .num_items = NUM_MENU_ITEMS,
    .items = data->menu_items,
  };
  
  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;
  
  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, 
                                              NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));
}


// =================================================================================
// Deinitialize resources on window unload that were initialized on window load
static void prv_window_unload(Window *window) {
  simple_menu_layer_destroy(s_app_data->menu_layer);
}


// =================================================================================
static void handle_init(void) {
  TestBTAppData *data = app_malloc_check(sizeof(TestBTAppData));
  memset(data, 0, sizeof(TestBTAppData));
  s_app_data = data;

  data->window = window_create();
  if (data->window == NULL) {
    return;
  }
  window_init(data->window, "");
  window_set_window_handlers(data->window, &(WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  app_window_stack_push(data->window, true /*animated*/);
}

static void handle_deinit(void) {
  // Don't bother freeing anything, the OS should be re-initing the heap.
}


// =================================================================================
static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

// =================================================================================
const PebbleProcessMd* test_bluetooth_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "Bluetooth Test"
  };
  return (const PebbleProcessMd*) &s_app_info;
}

