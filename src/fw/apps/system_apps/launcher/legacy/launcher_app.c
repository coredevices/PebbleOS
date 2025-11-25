/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "launcher_app.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "applib/legacy2/ui/menu_layer_legacy2.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_management/app_menu_data_source.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "shell/normal/app_idle_timeout.h"
#include "services/normal/notifications/do_not_disturb.h"
#include "services/normal/notifications/alerts_private.h"
#include "applib/ui/kino/kino_layer.h"
#include "system/passert.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct LauncherMenuData {
  Window window;
  StatusBarLayer status_bar;
  Layer status_bar_icons_layer;
  MenuLayer menu_layer;
  AppMenuDataSource data_source;

  EventServiceInfo battery_state_event_info;
  EventServiceInfo do_not_disturb_event_info;
  EventServiceInfo pebble_app_event_info;

  KinoLayer connectivity_icon;
  uint32_t connectivity_icon_id;
  KinoLayer battery_icon;
  uint32_t battery_icon_id;
} LauncherMenuData;

typedef struct LauncherMenuPersistedData {
  int scroll_offset_y;
  int menu_index_row;
  bool valid;
  RtcTicks leave_time;
} LauncherMenuPersistedData;

static LauncherMenuPersistedData s_launcher_menu_persisted_data;

///////////////////
// Status Bar

static bool prv_is_pebble_app_connected(void) {
  return (comm_session_get_system_session() != NULL);
}

static uint32_t prv_get_resource_id_for_battery_charge_state(BatteryChargeState charge_state) {
  const uint32_t battery_base_resource_id = (charge_state.is_charging || charge_state.is_plugged)
                                            ? RESOURCE_ID_TINTIN_LAUNCHER_CHARGING_5_PERCENT
                                            : RESOURCE_ID_TINTIN_LAUNCHER_BATTERY_5_PERCENT;
  if (charge_state.charge_percent <= 100) {
    return battery_base_resource_id + (charge_state.charge_percent / 10);
  } else {
    WTF;
  }
}

static void prv_reload_status_bar_icons(LauncherMenuData *data) {
  // Draw airplane mode, do not disturb, or silent status icon.
  AlertMask alert_mask = alerts_get_mask();

  // Get the connectivity ResourceId
  uint32_t new_connectivity_icon_id = RESOURCE_ID_INVALID;
  if (bt_ctl_is_airplane_mode_on()) {
    new_connectivity_icon_id = RESOURCE_ID_CONNECTIVITY_BLUETOOTH_AIRPLANE_MODE;
  } else if (do_not_disturb_is_active()) {
    new_connectivity_icon_id = RESOURCE_ID_CONNECTIVITY_BLUETOOTH_DND;
  } else if (!prv_is_pebble_app_connected()) {
    new_connectivity_icon_id = RESOURCE_ID_CONNECTIVITY_BLUETOOTH_DISCONNECTED;
  } else if (alert_mask != AlertMaskAllOn) {
    if (alert_mask == AlertMaskPhoneCalls) {
      new_connectivity_icon_id = RESOURCE_ID_CONNECTIVITY_BLUETOOTH_CALLS_ONLY;
    }
  } else if (prv_is_pebble_app_connected()) {
    new_connectivity_icon_id = RESOURCE_ID_CONNECTIVITY_BLUETOOTH_CONNECTED;
    // probably need an All Muted icon here
  }

  // replace the image if the connectivity ResourceId has changed
  if (data->connectivity_icon_id != new_connectivity_icon_id) {
    data->connectivity_icon_id = new_connectivity_icon_id;
    kino_layer_set_reel_with_resource(&data->connectivity_icon, data->connectivity_icon_id);
  }

  // Get the connectivity ResourceId
  const uint32_t new_battery_icon_id =
      prv_get_resource_id_for_battery_charge_state(battery_get_charge_state());

  // replace the image if the battery ResourceId has changed
  if (data->battery_icon_id != new_battery_icon_id) {
    data->battery_icon_id = new_battery_icon_id;
    kino_layer_set_reel_with_resource(&data->battery_icon, data->battery_icon_id);
  }
}

///////////////////
// Events

static void prv_event_handler(PebbleEvent *e, void *context) {
  LauncherMenuData *data = (LauncherMenuData *) context;
  prv_reload_status_bar_icons(data);
}

static void prv_subscribe_to_event(EventServiceInfo *result,
                                   PebbleEventType type,
                                   void *callback_context) {
  *result = (EventServiceInfo) {
    .type = type,
    .handler = prv_event_handler,
    .context = callback_context,
  };
  event_service_client_subscribe(result);
}

///////////////////
// AppMenuDataSource callbacks

static bool prv_app_filter_callback(struct AppMenuDataSource * const source,
                                    AppInstallEntry *entry) {
  if (app_install_entry_is_watchface(entry)
      || app_install_entry_is_hidden((entry))) {
    return false; // Skip watchfaces and hidden apps
  }
  return true;
}

static void prv_data_changed(void *context) {
  LauncherMenuData *data = context;
  menu_layer_reload_data(&data->menu_layer);
}

//////////////
// MenuLayer callbacks

static void select_callback(MenuLayer *menu_layer, MenuIndex *cell_index,
                            LauncherMenuData *data) {
  AppMenuNode *node = app_menu_data_source_get_node_at_index(&data->data_source, cell_index->row);
  app_manager_put_launch_app_event(&(AppLaunchEventConfig) {
    .id = node->install_id,
    .common.reason = APP_LAUNCH_USER,
    .common.button = BUTTON_ID_SELECT,
  });
}

static uint16_t get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index,
                                      LauncherMenuData *data) {
  return app_menu_data_source_get_count(&data->data_source);
}

static void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index,
                              LauncherMenuData *data) {
  app_menu_data_source_draw_row(&data->data_source, ctx, cell_layer, cell_index);
}

///////////////////
// Window callbacks

static void prv_window_load(Window *window) {
  LauncherMenuData *data = window_get_user_data(window);
  GRect bounds = window->layer.bounds;

  status_bar_layer_init(&data->status_bar);
  status_bar_layer_set_colors(&data->status_bar, GColorBlack, GColorWhite);
  layer_add_child(&window->layer, status_bar_layer_get_layer(&data->status_bar));

  static const int kino_width = 20;
  static const int kino_padding = 6;
  kino_layer_init(&data->connectivity_icon, &GRect(kino_padding, 0,
                                                   kino_width, STATUS_BAR_LAYER_HEIGHT));
  kino_layer_set_alignment(&data->connectivity_icon, GAlignLeft);
  layer_add_child(&window->layer, kino_layer_get_layer(&data->connectivity_icon));

  kino_layer_init(&data->battery_icon, &GRect(DISP_COLS - kino_width - kino_padding, 0,
                                             kino_width, STATUS_BAR_LAYER_HEIGHT));
  kino_layer_set_alignment(&data->battery_icon, GAlignRight);
  layer_add_child(&window->layer, kino_layer_get_layer(&data->battery_icon));

  prv_reload_status_bar_icons(data);

  bounds = grect_inset(bounds, GEdgeInsets(STATUS_BAR_LAYER_HEIGHT, 0, 0, 0));

  MenuLayer *menu_layer = &data->menu_layer;
  menu_layer_init(menu_layer, &bounds);
  app_menu_data_source_init(&data->data_source, &(AppMenuDataSourceCallbacks) {
    .changed = prv_data_changed,
    .filter = prv_app_filter_callback,
  }, data);

  app_menu_data_source_enable_icons(&data->data_source,
                                    RESOURCE_ID_MENU_LAYER_GENERIC_WATCHAPP_ICON);

  menu_layer_set_callbacks(menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) get_num_rows_callback,
    .draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
    .select_click = (MenuLayerSelectCallback) select_callback,
  });
  menu_layer_set_click_config_onto_window(menu_layer, window);
  layer_add_child(&window->layer, menu_layer_get_layer(menu_layer));

  scroll_layer_set_shadow_hidden(&data->menu_layer.scroll_layer, true);

  if (s_launcher_menu_persisted_data.valid) {
    // If we have a saved state, reload it.
    menu_layer_set_selected_index(&data->menu_layer,
                                  MenuIndex(0, s_launcher_menu_persisted_data.menu_index_row),
                                  MenuRowAlignNone,
                                  false);
    scroll_layer_set_content_offset(&data->menu_layer.scroll_layer,
                                    GPoint(0, s_launcher_menu_persisted_data.scroll_offset_y),
                                    false);
  } else {
    // If we are resetting the launcher, select the second entry (Settings is at the top)
    menu_layer_set_selected_index(&data->menu_layer, MenuIndex(0, 1), MenuRowAlignNone, false);
  }

  prv_subscribe_to_event(&data->battery_state_event_info, PEBBLE_BATTERY_STATE_CHANGE_EVENT, data);
  prv_subscribe_to_event(&data->do_not_disturb_event_info, PEBBLE_DO_NOT_DISTURB_EVENT, data);
  prv_subscribe_to_event(&data->pebble_app_event_info, PEBBLE_COMM_SESSION_EVENT, data);
}

static void prv_window_unload(Window *window) {
  LauncherMenuData *data = window_get_user_data(window);

  kino_layer_deinit(&data->connectivity_icon);
  kino_layer_deinit(&data->battery_icon);

  // Save the current state of the menu so we can restore it later.
  s_launcher_menu_persisted_data = (LauncherMenuPersistedData) {
    .valid = true,
    .scroll_offset_y = scroll_layer_get_content_offset(&data->menu_layer.scroll_layer).y,
    .menu_index_row = menu_layer_get_selected_index(&data->menu_layer).row,
    .leave_time = rtc_get_ticks(),
  };

  menu_layer_deinit(&data->menu_layer);
  app_menu_data_source_deinit(&data->data_source);
}


static void launcher_menu_push_window(void) {
  LauncherMenuData *data = app_zalloc(sizeof(LauncherMenuData));
  app_state_set_user_data(data);

  // Push launcher menu window:
  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Launcher Menu"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  const bool animated = false;
  app_window_stack_push(window, animated);
}

////////////////////
// App boilerplate

static void s_main(void) {
  const LauncherMenuArgs *args = (const LauncherMenuArgs *) app_manager_get_task_context()->args;
  if (args && args->reset_scroll) {
    if ((s_launcher_menu_persisted_data.leave_time + RETURN_TIMEOUT_TICKS) <= rtc_get_ticks()) {
      s_launcher_menu_persisted_data.valid = false;
    }
  }

  launcher_menu_push_window();

  app_idle_timeout_start();

  app_event_loop();
}

const PebbleProcessMd* launcher_menu_app_get_app_info() {
  static const PebbleProcessMdSystem s_launcher_menu_app_info = {
    .common = {
      .main_func = s_main,
      // UUID: dec0424c-0625-4878-b1f2-147e57e83688
      .uuid = {0xde, 0xc0, 0x42, 0x4c, 0x06, 0x25, 0x48, 0x78,
               0xb1, 0xf2, 0x14, 0x7e, 0x57, 0xe8, 0x36, 0x88},
      .visibility = ProcessVisibilityHidden
    },
    .name = "Launcher",
  };
  return (const PebbleProcessMd*) &s_launcher_menu_app_info;
}
