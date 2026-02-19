/* SPDX-License-Identifier: Apache-2.0 */

#include "two_clicks_launcher.h"

#include "applib/app.h"
#include "applib/ui/vibes.h"
#include "apps/system_app_ids.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "shell/normal/quick_launch.h"
#include "util/size.h"
#include "util/uuid.h"

#include <stdio.h>

static TwoClicksAppData *s_app_data;

static void update_proc(Layer *layer, GContext *ctx) {
  GPoint origin = GPoint(5, 15);
  GDrawCommandImage *image;
  if (layer == s_app_data->app_up.icon_layer) {
    image = s_app_data->app_up.icon_image;
  } else if (layer == s_app_data->app_select.icon_layer) {
    image = s_app_data->app_select.icon_image;
  } else if (layer == s_app_data->app_down.icon_layer) {
    image = s_app_data->app_down.icon_image;
  } else {
    image = s_app_data->app_up.icon_image;
  }

  gdraw_command_image_draw(ctx, image, origin);
}

/// --- AppGraphicNode ---

static void prv_agn_init(AppGraphicNode *node, ButtonId button_id, TwoClicksAppData *data) {
  AppInstallEntry entry;
  uint32_t action_icon_resource_id;
  AppInstallId app_id;
  Layer *window_layer = window_get_root_layer(&data->window);
  GRect window_frame = layer_get_bounds_by_value(window_layer);
  GRect text_frame;
  GRect icon_frame;
  switch (button_id) {
    case BUTTON_ID_UP:
      action_icon_resource_id = RESOURCE_ID_ACTION_BAR_ICON_UP;
      app_id = data->app_up_id;
      text_frame = GRect(ICON_WIDTH + ICON_MARGIN, TEXT_VERTICAL_OFFSET + 5, window_frame.size.w - ICON_WIDTH - ICON_MARGIN - ACTION_BAR_WIDTH, window_frame.size.h / 3 - TEXT_VERTICAL_OFFSET - 5);
      icon_frame = GRect(0, 5, ICON_WIDTH, (window_frame.size.h / 3) - 5);
      break;
    case BUTTON_ID_SELECT:
      action_icon_resource_id = RESOURCE_ID_ACTION_BAR_ICON_START;
      app_id = data->app_select_id;
      text_frame = GRect(ICON_WIDTH + ICON_MARGIN, TEXT_VERTICAL_OFFSET + window_frame.size.h / 3, window_frame.size.w - ICON_WIDTH - ICON_MARGIN - ACTION_BAR_WIDTH, window_frame.size.h / 3 - TEXT_VERTICAL_OFFSET);
      icon_frame = GRect(0, window_frame.size.h / 3, ICON_WIDTH, window_frame.size.h / 3);
      break;
    case BUTTON_ID_DOWN:
      action_icon_resource_id = RESOURCE_ID_ACTION_BAR_ICON_DOWN;
      app_id = data->app_down_id;
      text_frame = GRect(ICON_WIDTH + ICON_MARGIN, TEXT_VERTICAL_OFFSET - 5 + window_frame.size.h * 2 / 3, window_frame.size.w - ICON_WIDTH - ICON_MARGIN - ACTION_BAR_WIDTH, window_frame.size.h / 3 - TEXT_VERTICAL_OFFSET + 5);
      icon_frame = GRect(0, (window_frame.size.h * 2 / 3) - 5, ICON_WIDTH, (window_frame.size.h / 3) + 5);
      break;
    default:
      node->enabled = false;
      return;
  }
  if (!app_install_get_entry_for_install_id(app_id, &entry)) {
    node->enabled = false;
    return;
  }
  memcpy(node->name, entry.name, NAME_BUFFER_SIZE);
  
  node->action_icon_bitmap = gbitmap_create_with_resource_system(SYSTEM_APP, action_icon_resource_id);

  node->icon_image = gdraw_command_image_create_with_resource_system(app_install_get_app_icon_bank(&entry), app_install_entry_get_icon_resource_id(&entry));
  node->icon_bitmap = gbitmap_create_with_resource_system(app_install_get_app_icon_bank(&entry), app_install_entry_get_icon_resource_id(&entry));
  if (node->icon_image != NULL) {
    node->icon_layer = layer_create(icon_frame);
    layer_set_update_proc(node->icon_layer, update_proc);
  } else {
    if (node->icon_bitmap == NULL) {
      node->icon_bitmap = gbitmap_create_with_resource_system(SYSTEM_APP, RESOURCE_ID_MENU_LAYER_GENERIC_WATCHAPP_ICON);
    }
    node->icon_bitmap_layer = bitmap_layer_create(icon_frame);
    bitmap_layer_set_bitmap(node->icon_bitmap_layer, node->icon_bitmap);
    node->icon_layer = bitmap_layer_get_layer(node->icon_bitmap_layer);
  }

  TextLayer *text_layer = node->name_layer;
  text_layer = text_layer_create(text_frame);
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(text_layer, (const char *)node->name);
  text_layer_set_overflow_mode(text_layer, GTextOverflowModeTrailingEllipsis);

  layer_add_child(window_layer, node->icon_layer);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  node->enabled = true;
}

static void prv_agn_deinit(AppGraphicNode *node) {
  node->enabled = false;

  if (node->icon_bitmap != NULL) {
    bitmap_layer_destroy(node->icon_bitmap_layer);
    gbitmap_destroy(node->icon_bitmap);
  } else {
    layer_destroy(node->icon_layer);
    gdraw_command_image_destroy(node->icon_image);
  }
  gbitmap_destroy(node->action_icon_bitmap);
  text_layer_destroy(node->name_layer);
}

// -------

static void prv_vibe_pulse(void) {
  uint32_t const segments[] = { 150 };
  VibePattern pat = {
    .durations = segments,
    .num_segments = ARRAY_LENGTH(segments),
  };
  vibes_enqueue_custom_pattern(pat);
}

static void prv_inactive_timer_callack(void *data) {
  (void)data;
  app_window_stack_pop(true);
}

static void prv_inactive_timer_refresh(TwoClicksAppData *data) {
  static const uint32_t INACTIVITY_TIMEOUT_MS = 10 * 1000;
  data->inactive_timer_id = evented_timer_register_or_reschedule(
      data->inactive_timer_id, INACTIVITY_TIMEOUT_MS, prv_inactive_timer_callack, data);
}

static void prv_cleanup_timer(EventedTimerID *timer) {
  if (evented_timer_exists(*timer)) {
    evented_timer_cancel(*timer);
    *timer = EVENTED_TIMER_INVALID_ID;
  }
}

static void prv_click_handler(ClickRecognizerRef recognizer, void *context) {
  TwoClicksAppData* data = context;
  AppInstallId app_id = INSTALL_ID_INVALID;
  bool is_enabled = false;
  ButtonId second_button_id = click_recognizer_get_button_id(recognizer);
  
  if (data->args->is_tap) {
    app_id = quick_launch_two_clicks_tap_get_app(data->args->first_button, second_button_id);
    is_enabled = quick_launch_two_clicks_tap_is_enabled(data->args->first_button, second_button_id);
  } else {
    app_id = quick_launch_two_clicks_get_app(data->args->first_button, second_button_id);
    is_enabled = quick_launch_two_clicks_is_enabled(data->args->first_button, second_button_id);
  }

  if (!is_enabled || app_id == INSTALL_ID_INVALID) {
    return;
  }

  AppLaunchEventConfig config = (AppLaunchEventConfig) {
    .id = app_id,
    .common.reason = APP_LAUNCH_QUICK_LAUNCH,
    .common.button = second_button_id,
  };
  app_manager_put_launch_app_event(&config);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click_handler);
}

static void prv_two_clicks_window_load(Window *window) {
  window_set_background_color(window, GColorWhite);
  TwoClicksAppData *data = window_get_user_data(window);

  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds_by_value(window_layer);

  // App
  if (data->app_up_id != INSTALL_ID_INVALID) {
    prv_agn_init(&data->app_up, BUTTON_ID_UP, data);
  }
  if (data->app_select_id != INSTALL_ID_INVALID) {
    prv_agn_init(&data->app_select, BUTTON_ID_SELECT, data);
  }
  if (data->app_down_id != INSTALL_ID_INVALID) {
    prv_agn_init(&data->app_down, BUTTON_ID_DOWN, data);
  }

  // ActionBar
  ActionBarLayer *action_bar = &data->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_add_to_window(action_bar, window);
  action_bar_layer_set_context(action_bar, data);
  action_bar_layer_set_click_config_provider(action_bar, prv_click_config_provider);
  action_bar_layer_add_to_window(action_bar, window);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, data->app_up.action_icon_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, data->app_select.action_icon_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, data->app_down.action_icon_bitmap);
}

static void prv_two_clicks_window_appear(Window *window) {
  TwoClicksAppData *data = window_get_user_data(window);

  // re-enable the inactivity timer back in 2-clicks view
  prv_inactive_timer_refresh(data);

  if (data->args->vibe_on_start) {
    prv_vibe_pulse();
  }
}

static void prv_two_clicks_window_disappear(Window *window) {
  TwoClicksAppData *data = window_get_user_data(window);

  // disable the inactivity timer when the user leaves
  prv_cleanup_timer(&data->inactive_timer_id);
}

static void prv_two_clicks_window_unload(Window *window) {
  TwoClicksAppData *data = window_get_user_data(window);

  prv_agn_deinit(&data->app_up);
  prv_agn_deinit(&data->app_select);
  prv_agn_deinit(&data->app_down);
  action_bar_layer_deinit(&data->action_bar);
}

static void prv_init() {
  TwoClicksAppData *data = app_malloc_check(sizeof(TwoClicksAppData));
  s_app_data = data;
  *data = (TwoClicksAppData){};

  const TwoClicksArgs *args = process_manager_get_current_process_args();
  data->args = args;

  if (data->args->is_tap) {
    data->app_up_id = quick_launch_two_clicks_tap_get_app(data->args->first_button, BUTTON_ID_UP);
    data->app_select_id = quick_launch_two_clicks_tap_get_app(data->args->first_button, BUTTON_ID_SELECT);
    data->app_down_id = quick_launch_two_clicks_tap_get_app(data->args->first_button, BUTTON_ID_DOWN);
  } else {
    data->app_up_id = quick_launch_two_clicks_get_app(data->args->first_button, BUTTON_ID_UP);
    data->app_select_id = quick_launch_two_clicks_get_app(data->args->first_button, BUTTON_ID_SELECT);
    data->app_down_id = quick_launch_two_clicks_get_app(data->args->first_button, BUTTON_ID_DOWN);
  }

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("2-Clicks"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_two_clicks_window_load,
    .appear = prv_two_clicks_window_appear,
    .disappear = prv_two_clicks_window_disappear,
    .unload = prv_two_clicks_window_unload
  });

  app_window_stack_push(window, true /* animated */);
}

static void prv_deinit() {
  prv_cleanup_timer(&s_app_data->inactive_timer_id);
  app_free(s_app_data);
}

static void prv_main() {
  prv_init();

  app_event_loop();

  prv_deinit();
}

const PebbleProcessMd *two_clicks_launcher_get_app_info() {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      .main_func = prv_main,
      .uuid = TWO_CLICKS_LAUNCHER_UUID_INIT,
      .visibility = ProcessVisibilityQuickLaunch,
    },
    .name = i18n_noop("2-Clicks"),
  };
  return &s_app_md.common;
}