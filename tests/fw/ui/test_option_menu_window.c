/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/option_menu_window.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/timeline_resources.h"

#include "clar.h"

// Fakes
/////////////////////

#include "fake_app_state.h"
#include "fake_content_indicator.h"
#include "fake_graphics_context.h"
#include "fixtures/load_test_resources.h"

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_animation_timing.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_bootbits.h"
#include "stubs_buffer.h"
#include "stubs_click.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_event_service_client.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_unobstructed_area.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

// Setup and Teardown
////////////////////////////////////

typedef struct OptionMenuTestData {
  OptionMenu option_menu;
} OptionMenuTestData;

OptionMenuTestData s_data;

void test_option_menu_window__initialize(void) {
  fake_app_state_init();
  load_system_resources_fixture();

  s_data = (OptionMenuTestData) {};
  rtc_set_time(3 * SECONDS_PER_DAY);
}

void test_option_menu_window__cleanup(void) {
}

// Helpers
//////////////////////

typedef struct MenuItemConfig {
  const char *title;
  const char *subtitle;
} MenuItemConfig;

typedef struct MenuConfig {
  OptionMenuCallbacks callbacks;
  const char *title;
  size_t num_items;
  MenuItemConfig *items;
  OptionMenuContentType content_type;
  bool icons_enabled;
} MenuConfig;

static uint16_t prv_menu_get_num_rows(OptionMenu *option_menu, void *context) {
  return ((MenuConfig *)context)->num_items;
}

static void prv_menu_draw_row(OptionMenu *option_menu, GContext *ctx, const Layer *cell_layer,
                              const GRect *cell_frame, uint32_t row, bool selected, void *context) {
  MenuConfig *data = context;
  option_menu_system_draw_row(option_menu, ctx, cell_layer, cell_frame, data->items[row].title,
                              selected, context);
}

static void prv_create_menu_and_render(MenuConfig *config) {
  option_menu_init(&s_data.option_menu);

  const OptionMenuConfig option_menu_config = {
    .title = config->title ?: "Option Menu",
    .content_type = config->content_type,
    .status_colors = { GColorWhite, GColorBlack },
    .highlight_colors = { PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack), GColorWhite },
    .icons_enabled = config->icons_enabled,
  };
  option_menu_configure(&s_data.option_menu, &option_menu_config);

  const OptionMenuCallbacks callbacks = {
    .draw_row = config->callbacks.draw_row ?: prv_menu_draw_row,
    .get_num_rows = config->callbacks.get_num_rows ?: prv_menu_get_num_rows,
    .get_cell_height = config->callbacks.get_cell_height ?: NULL,
  };
  option_menu_set_callbacks(&s_data.option_menu, &callbacks, config);

  window_set_on_screen(&s_data.option_menu.window, true, true);
  window_render(&s_data.option_menu.window, fake_graphics_context_get_context());
}

// Tests
//////////////////////

// These tests test all permutations on all platforms even if the combination on the particular
// platform was not designed and thus does not appear pleasant. Make sure you are looking at the
// combination relevant to your use case when examining the unit test output of Option Menu.

void prv_create_menu_and_render_long_title(bool icons_enabled, const char *title,
                                           bool special_height) {
  prv_create_menu_and_render(&(MenuConfig) {
    .title = title,
    .content_type = special_height ? OptionMenuContentType_DoubleLine :
                                     OptionMenuContentType_Default,
    .num_items = 3,
    .items = (MenuItemConfig[]) {
      {
        .title = "Allow All Notifications",
      }, {
        .title = "Allow Phone Calls Only",
      }, {
        .title = "Mute All Notifications",
      }
    },
    .icons_enabled = icons_enabled,
  });
}

void test_option_menu_window__long_title_default_height(void) {
  prv_create_menu_and_render_long_title(false /* icons_enabled */, "Default Height",
                                        false /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void test_option_menu_window__long_title_default_height_icons(void) {
  prv_create_menu_and_render_long_title(true /* icons_enabled */, "Default Height",
                                        false /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void test_option_menu_window__long_title_special_height(void) {
  prv_create_menu_and_render_long_title(false /* icons_enabled */, "Special Height",
                                        true /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void test_option_menu_window__long_title_special_height_icons(void) {
  prv_create_menu_and_render_long_title(true /* icons_enabled */, "Special Height",
                                        true /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void prv_create_menu_and_render_short_title(bool icons_enabled, const char *title,
                                            bool special_height) {
  prv_create_menu_and_render(&(MenuConfig) {
    .title = title,
    .content_type = special_height ? OptionMenuContentType_SingleLine :
                                     OptionMenuContentType_Default,
    .num_items = 3,
    .items = (MenuItemConfig[]) {
      {
        .title = "Smaller",
      }, {
        .title = "Default",
      }, {
        .title = "Larger",
      }
    },
    .icons_enabled = icons_enabled,
  });
}

void test_option_menu_window__short_title_default_height(void) {
  prv_create_menu_and_render_short_title(false /* icons_enabled */, "Default Height",
                                         false /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void test_option_menu_window__short_title_default_height_icons(void) {
  prv_create_menu_and_render_short_title(true /* icons_enabled */, "Default Height",
                                         false /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void test_option_menu_window__short_title_special_height(void) {
  prv_create_menu_and_render_short_title(false /* icons_enabled */, "Special Height",
                                         true /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void test_option_menu_window__short_title_special_height_icons(void) {
  prv_create_menu_and_render_short_title(true /* icons_enabled */, "Special Height",
                                         true /* special_height */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}
