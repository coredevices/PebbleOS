/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_fs_resources.h"

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "system/passert.h"

typedef struct {
  Window window;
  GBitmap bitmap;
  BitmapLayer bitmap_layer;
} FSResourceAppData;

static void prv_window_load(Window *window) {
  FSResourceAppData *data = window_get_user_data(window);
  Layer *root_layer = window_get_root_layer(window);
  gbitmap_init_with_resource(&data->bitmap, RESOURCE_ID_PUG);
  bitmap_layer_init(&data->bitmap_layer, &root_layer->bounds);
  bitmap_layer_set_bitmap(&data->bitmap_layer, &data->bitmap);
  layer_add_child(root_layer, bitmap_layer_get_layer(&data->bitmap_layer));
}

static void push_window(FSResourceAppData *data) {
  Window *window = &data->window;
  window_init(window, WINDOW_NAME("FS Resource Demo"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}

////////////////////
// App boilerplate
static void handle_init(void) {
  FSResourceAppData *data = (FSResourceAppData*) app_malloc_check(sizeof(FSResourceAppData));
  if (data == NULL) {
    PBL_CROAK("Out of memory");
  }
  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit(void) {
  FSResourceAppData *data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* fs_resources_app_get_info() {
  static const PebbleProcessMdSystem s_fs_resources_app_info = {
    .common.main_func = s_main,
    .name = "FS Resources"
  };
  return (const PebbleProcessMd*) &s_fs_resources_app_info;
}


