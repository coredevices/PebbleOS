/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_heap_demo.h"
#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"

#include <stdio.h>

// This app allocated approximately 75% of memory available to it.
// The idea is to run it multiple times to show that all data is being freed on
// exit, and is available for the next app to use.

static TextLayer *text_heap_info;
static Window *window;

static void init(void) {
  char *start = app_malloc_check(1); // Get a pointer close to where the heap starts

  window = window_create();
  app_window_stack_push(window, true /* Animated */);
  Layer *window_layer = window_get_root_layer(window);

  text_heap_info = text_layer_create(window_layer->frame);
  text_layer_set_text_color(text_heap_info, GColorWhite);
  text_layer_set_background_color(text_heap_info, GColorBlack);
  text_layer_set_font(text_heap_info, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

  // Get the size of the heap from the beginning of the first thing allocated
  unsigned long heap_size = 0x20020000 - (unsigned long)start;
  unsigned long alloc_size = 0.75*heap_size;

  char *buf = app_malloc_check(alloc_size);
  snprintf(buf, 80, "%luB/%luB\n\nJust allocated %lu%% of the app heap.", alloc_size, heap_size, 100*alloc_size/heap_size);
  text_layer_set_text(text_heap_info, buf);
  layer_add_child(window_layer, text_layer_get_layer(text_heap_info));
}

static void deinit(void) {
  // Don't free anything
}

static void s_main(void) {
  init();
  app_event_loop();
  deinit();
}

const PebbleProcessMd* app_heap_demo_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_heap_demo_app_info = {
    .common.main_func = &s_main,
    .name = "AppHeap"
  };
  return (const PebbleProcessMd*) &s_app_heap_demo_app_info;
}
