/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "emoji_selection_demo.h"
#include "menu.h"

#include "applib/app_timer.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/action_menu_hierarchy.h"
#include "applib/ui/action_menu_window.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/ui.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/util/size.h"

#include <stdio.h>
#include <string.h>

// Default set kept in sync with prv_create_emoji_level_from_action() in
// src/fw/services/timeline/timeline_actions.c. The order backend below stays
// as the single source for this demo so a future change can wire the real
// picker and this demo to a shared, user-configurable order.
static const char *s_default_emoji[] = {
  "😃", "😉", "😂", "😍", "😘", "❤",  "😇", "😎", "😛", "😟", "😩",
  "😭", "😴", "😐", "😯", "👍", "👎", "👌", "💩", "🎉", "🍺",
};

#define EMOJI_DEMO_MAX 40

static const char *s_current_order[EMOJI_DEMO_MAX];
static uint16_t s_current_count;
static bool s_initialized;

static void prv_ensure_init(void) {
  if (s_initialized) {
    return;
  }
  for (uint16_t i = 0; i < ARRAY_LENGTH(s_default_emoji); i++) {
    s_current_order[i] = s_default_emoji[i];
  }
  s_current_count = ARRAY_LENGTH(s_default_emoji);
  s_initialized = true;
}

static uint16_t prv_emoji_count(void) {
  prv_ensure_init();
  return s_current_count;
}

static const char *prv_emoji_at(uint16_t index) {
  prv_ensure_init();
  return (index < s_current_count) ? s_current_order[index] : "";
}

typedef struct {
  Window window;
  TextLayer header_layer;
  TextLayer emoji_layer;
  char header[16];
  char emoji[16];
} ResultData;

static void prv_result_load(Window *window) {
  ResultData *data = window_get_user_data(window);
  const GRect bounds = window->layer.bounds;

  text_layer_init(&data->header_layer, &GRect(0, 2, bounds.size.w, 18));
  text_layer_set_text_alignment(&data->header_layer, GTextAlignmentCenter);
  text_layer_set_font(&data->header_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text(&data->header_layer, data->header);
  layer_add_child(&window->layer, &data->header_layer.layer);

  const int16_t body_y = 30;
  text_layer_init(&data->emoji_layer, &GRect(0, body_y, bounds.size.w, bounds.size.h - body_y));
  text_layer_set_text_alignment(&data->emoji_layer, GTextAlignmentCenter);
  text_layer_set_font(&data->emoji_layer,
#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 8
                      fonts_get_system_font(FONT_KEY_GOTHIC_44_EMOJI_PICKER_COLOR));
#else
                      fonts_get_system_font(FONT_KEY_GOTHIC_40_EMOJI_PICKER));
#endif
  text_layer_set_text(&data->emoji_layer, data->emoji);
  layer_add_child(&window->layer, &data->emoji_layer.layer);
}

static void prv_result_unload(Window *window) {
  ResultData *data = window_get_user_data(window);
  app_free(data);
}

// Result window shown when the picker closes, so the pick is visible.
static ResultData *prv_result_create(const char *emoji, uint16_t index, uint16_t count) {
  ResultData *data = app_zalloc_check(sizeof(*data));
  snprintf(data->emoji, sizeof(data->emoji), "%s", emoji);
  snprintf(data->header, sizeof(data->header), "%u of %u", index + 1, count);
  window_init(&data->window, WINDOW_NAME("Emoji Picked"));
  window_set_user_data(&data->window, data);
  window_set_background_color(&data->window, GColorWhite);
  window_set_window_handlers(&data->window, &(WindowHandlers){
                                              .load = prv_result_load,
                                              .unload = prv_result_unload,
                                            });
  return data;
}

typedef struct {
  Window window;
  ActionMenu *menu;
  ActionMenuLevel *root;
  AppTimer *open_timer;
  bool menu_open;
} DemoData;

// Demo pick: show the chosen emoji as the menu result, mirroring the reply
// flow's result window without sending anything.
static void prv_pick_cb(ActionMenu *menu, const ActionMenuItem *item, void *context) {
  const char *label = action_menu_item_get_label(item);
  if (!label) {
    return;
  }
  uint16_t index = 0;
  for (uint16_t i = 0; i < prv_emoji_count(); i++) {
    if (prv_emoji_at(i) == label) {
      index = i;
      break;
    }
  }
  ResultData *result = prv_result_create(label, index, prv_emoji_count());
  if (result) {
    action_menu_set_result_window(menu, &result->window);
  }
}

static void prv_menu_did_close(ActionMenu *menu, const ActionMenuItem *performed_action,
                               void *context) {
  DemoData *data = context;
  if (data->root) {
    action_menu_hierarchy_destroy(data->root, NULL, NULL);
    data->root = NULL;
  }
  data->menu = NULL;
  app_window_stack_remove(&data->window, true /* animated */);
}

static void prv_open_picker(DemoData *data) {
  const uint16_t count = prv_emoji_count();
  ActionMenuLevel *root = action_menu_level_create(count);
  action_menu_level_set_display_mode(root, ActionMenuLevelDisplayModeThin);
  for (uint16_t i = 0; i < count; i++) {
    action_menu_level_add_action(root, prv_emoji_at(i), prv_pick_cb, NULL);
  }
  data->root = root;
  ActionMenuConfig config = {
    .root_level = root,
    .context = data,
    .did_close = prv_menu_did_close,
  };
  data->menu = app_action_menu_open(&config);
  data->menu_open = true;
}

// Delay before opening the picker so the launcher window push completes first.
#define OPEN_DELAY_MS 100

static void prv_open_timer_cb(void *context) {
  DemoData *data = context;
  data->open_timer = NULL;
  prv_open_picker(data);
}

static void prv_window_appear(Window *window) {
  DemoData *data = window_get_user_data(window);
  if (!data->menu_open && !data->open_timer) {
    // Defer past the in-progress push: opening the picker reentrantly from
    // appear leaves button routing on this window, so the picker renders
    // but never receives input.
    data->open_timer = app_timer_register(OPEN_DELAY_MS, prv_open_timer_cb, data);
  } else if (data->menu_open && !data->menu) {
    app_window_stack_remove(&data->window, true /* animated */);
  }
}

static void prv_window_unload(Window *window) {
  DemoData *data = window_get_user_data(window);
  if (data->open_timer) {
    app_timer_cancel(data->open_timer);
  }
  app_free(data);
}

static Window *prv_init(void) {
  prv_ensure_init();
  DemoData *data = app_zalloc_check(sizeof(*data));
  window_init(&data->window, WINDOW_NAME("Emoji Selection Demo"));
  window_set_user_data(&data->window, data);
  window_set_background_color(&data->window, GColorBlack);
  window_set_window_handlers(&data->window, &(WindowHandlers){
                                              .appear = prv_window_appear,
                                              .unload = prv_window_unload,
                                            });
  return &data->window;
}

const SettingsModuleMetadata *settings_emoji_selection_demo_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Emoji Selection Demo"),
    .init = prv_init,
  };
  return &s_module_info;
}
