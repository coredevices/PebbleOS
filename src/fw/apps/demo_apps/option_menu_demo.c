/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "option_menu_demo.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/option_menu_window.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "system/logging.h"
#include "util/size.h"

const char *s_strings[] = {
  "One",
  "Two",
  "Three",
  "Four",
};

static void prv_menu_select(OptionMenu *option_menu, int selection, void *context) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Option Menu Demo: selected %d", selection);
}

static uint16_t prv_menu_get_num_rows(OptionMenu *option_menu, void *context) {
  return ARRAY_LENGTH(s_strings);
}

static void prv_menu_draw_row(OptionMenu *option_menu, GContext *ctx, const Layer *cell_layer,
                              const GRect *text_frame, uint32_t row, bool selected, void *context) {
  option_menu_system_draw_row(option_menu, ctx, cell_layer, text_frame, s_strings[row], selected,
                              context);
}

static void prv_menu_unload(OptionMenu *option_menu, void *context) {
  option_menu_destroy(option_menu);
}

static void prv_init(void) {
  OptionMenu *option_menu = option_menu_create();

  const OptionMenuConfig config = {
    .title = "Option Menu",
    .choice = OPTION_MENU_CHOICE_NONE,
    .status_colors = { GColorDarkGray, GColorWhite },
    .highlight_colors = { PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack), GColorWhite },
    .icons_enabled = true
  };
  option_menu_configure(option_menu, &config);
  option_menu_set_callbacks(option_menu, &(OptionMenuCallbacks) {
    .select = prv_menu_select,
    .get_num_rows = prv_menu_get_num_rows,
    .draw_row = prv_menu_draw_row,
    .unload = prv_menu_unload,
  }, option_menu);

  const bool animated = true;
  app_window_stack_push(&option_menu->window, animated);
}

static void prv_deinit(void) {
}

///////////////////////////
// App boilerplate
///////////////////////////

static void s_main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd *option_menu_demo_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common = {
      .main_func = s_main,
      // UUID: e8f5d3cc-76ad-4575-97da-6d2049a1b3a4
      .uuid = {0xe8, 0xf5, 0xd3, 0xcc, 0x76, 0xad, 0x45, 0x75,
               0x97, 0xda, 0x6d, 0x20, 0x49, 0xa1, 0xb3, 0xa4},
    },
    .name = "Option Menu Demo",
  };

  return (const PebbleProcessMd*) &s_app_info;
}
