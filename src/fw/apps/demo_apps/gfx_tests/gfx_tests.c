/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gfx_tests.h"

#include "applib/ui/app_window_stack.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"
#include "system/profiler.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct {
  Window menu_window;
  Window test_window;
  Window results_window;
  MenuLayer test_menu;
  TextLayer results_text;
  char results_str[100];
  GfxTest *current_test;
} AppData;

#define RAND_SEED     (775762732)   // Randomly selected
#define US_PER_MS     (1000)
#define US_PER_S      (1000 * 1000)
#define TARGET_FPS    (30)
#define US_PER_FRAME  (20 * US_PER_MS)    // Upper bound on amount of time available to the rest of
                                          // the system while a frame is being pushed out to the
                                          // display with the cpu clock at 64MHz

#define GFX_TEST(name) extern GfxTest g_gfx_test_##name;
#include "gfx_test_list.h"
#undef GFX_TEST
#define GFX_TEST(name) &g_gfx_test_##name,
static GfxTest *s_tests[] = {
#include "gfx_test_list.h"
};

static void prv_pop_test_window(void *data) {
  AppData *app_data = data;
  app_window_stack_pop(false);
  app_window_stack_push(&app_data->results_window, false);
}

static void prv_test_update_proc(Layer *layer, GContext* ctx) {
  AppData *app_data = window_get_user_data(layer->window);
  GfxTest *test = app_data->current_test;

  srand(RAND_SEED); // Setup rand for routines that need it

  if (test->setup) {
    test->setup(layer->window);
  }

  PROFILER_INIT;
  PROFILER_START;
  while (PROFILER_NODE_GET_TOTAL_US(gfx_test_update_proc) < (test->duration * US_PER_S)) {
    PROFILER_NODE_START(gfx_test_update_proc);
    test->test_proc(layer, ctx);
    PROFILER_NODE_STOP(gfx_test_update_proc);
  }
  PROFILER_STOP;
  PROFILER_PRINT_STATS;

  if (test->teardown) {
    test->teardown(layer->window);
  }

  app_timer_register(0, prv_pop_test_window, app_data);
}

static void prv_start_test(GfxTest *test, AppData *app_data) {
  app_data->current_test = test;
  Window *window = &app_data->test_window;
  window_init(window, WINDOW_NAME(test->name));
  window_set_user_data(window, app_data);
  window_set_fullscreen(window, true);
  layer_set_update_proc((Layer *) window, prv_test_update_proc);
  app_window_stack_push(window, false);
}

static uint16_t prv_get_num_rows(struct MenuLayer *menu_layer, uint16_t section,
                                 void *callback_context) {
  return (uint16_t) ARRAY_LENGTH(s_tests);
}

static void prv_draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index,
                         void *callback_context) {
  PBL_ASSERTN(cell_index->row < ARRAY_LENGTH(s_tests));
  menu_cell_title_draw(ctx, cell_layer, s_tests[cell_index->row]->name);
}

static void prv_click_handler(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                              void *callback_context) {
  PBL_ASSERTN(cell_index->row < ARRAY_LENGTH(s_tests));
  prv_start_test(s_tests[cell_index->row], (AppData *)callback_context);
}

static void prv_handle_results_click(ClickRecognizerRef recognizer, void *context) {
  app_window_stack_pop(false);
}

static void prv_results_window_load(Window *window) {
  AppData *app_data = window_get_user_data(window);

  uint32_t total_us = PROFILER_NODE_GET_TOTAL_US(gfx_test_update_proc);
  uint32_t count = PROFILER_NODE_GET_COUNT(gfx_test_update_proc);

  GfxTest *test = app_data->current_test;

  uint32_t avg_us = (10 * total_us) / count;  // multiply by 10 to get decimals
  uint32_t per_frame = (100 * US_PER_FRAME) / avg_us; // multiply by 100 to get decimals and account
                                                      // for x10 in avg_us calc
  uint32_t fps = (TARGET_FPS * 100 * US_PER_FRAME) / avg_us;
  uint32_t unit_per_frame = ((uint64_t)test->unit_multiple * US_PER_FRAME * 100) / avg_us;

  snprintf(app_data->results_str, sizeof(app_data->results_str),
      "%10s\n"
      "Avg (µs):\n%"PRIu32".%"PRIu32"\n"
      "FPS:\n%"PRIu32".%"PRIu32"\n"
      "Per frame @ 30fps:\n%"PRIu32".%"PRIu32"\n"
      "Units per frame @ 30fps:\n%"PRIu32".%"PRIu32,
      test->name, avg_us / 10, avg_us % 10, fps / 10, fps % 10, per_frame / 10, per_frame % 10,
      unit_per_frame / 10, unit_per_frame % 10);
  PBL_LOG(LOG_LEVEL_DEBUG, "results: %s", app_data->results_str);
  text_layer_set_text(&app_data->results_text, app_data->results_str);
}

static void prv_results_window_unload(Window *window) {
  AppData *app_data = window_get_user_data(window);
  menu_layer_deinit(&app_data->test_menu);
}

static void prv_results_window_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_handle_results_click);
}

static void handle_init(void) {
  AppData *app_data = app_malloc_check(sizeof(AppData));

  // Menu window
  Window *window = &app_data->menu_window;
  window_init(window, WINDOW_NAME("GFX Test Framework"));
  window_set_user_data(window, app_data);
  window_set_fullscreen(window, false);

  MenuLayer *menu = &app_data->test_menu;
  menu_layer_init(menu, &window->layer.bounds);
  menu_layer_set_callbacks(menu, app_data, &(MenuLayerCallbacks){
    .get_num_rows = prv_get_num_rows,
    .draw_row = prv_draw_row,
    .select_click = prv_click_handler,
  });
  menu_layer_set_click_config_onto_window(menu, window);
  layer_add_child((Layer *) window, (Layer *) menu);
  app_window_stack_push(window, true);

  // Results window
  window = &app_data->results_window;
  window_init(window, WINDOW_NAME("Test Results"));
  window_set_user_data(window, app_data);
  window_set_fullscreen(window, false);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_results_window_load,
    .unload = prv_results_window_unload,
  });
  window_set_click_config_provider(window, prv_results_window_click_config_provider);

  TextLayer *text = &app_data->results_text;
  text_layer_init(text, &window->layer.bounds);
  text_layer_set_text(text, "");
  layer_add_child((Layer *) window, (Layer *) text);
}

static void s_main(void) {
  handle_init();
  app_event_loop();
}

const PebbleProcessMd* gfx_tests_get_app_info() {
  static const PebbleProcessMdSystem gfx_tests_app_info = {
    .name = "GFX Tests",
    .common = {
        .main_func = &s_main,
       // UUID: 06a8126b-d805-4197-af6d-8df3c1efb8e4
        .uuid = { 0x06, 0xa8, 0x12, 0x6b, 0xd8, 0x05, 0x41, 0x97, 0xaf, 0x6d, 0x8d, 0xf3,
          0xc1, 0xef, 0xb8, 0xe4},
    }
  };
  return (const PebbleProcessMd*) &gfx_tests_app_info;
}
