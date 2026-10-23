/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_glance_settings.h"

#include "app_glance_structured.h"
#include "menu_layer.h"

#include <stdio.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>
#include <pbl/services/battery/battery_state.h>
#include <pbl/services/bluetooth/ble_hrm.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/notifications/alerts_private.h>
#include <pbl/services/notifications/do_not_disturb.h>
#include <pbl/util/size.h>
#include <pbl/util/struct.h>

#include <applib/battery_state_service.h>
#include <applib/graphics/gpath.h>
#include <applib/graphics/text_resources.h>
#include <apps/system/timeline/text_node.h>
#include <kernel/events.h>
#include <kernel/pbl_malloc.h>
#include <process_management/app_install_manager.h>
#include <resource/resource_ids.auto.h>

//! Horizontal span of one row of the charging bolt, top to bottom
typedef struct ChargingIconRow {
  uint8_t x;
  uint8_t w;
} ChargingIconRow;

static const ChargingIconRow s_charging_icon_rows_small[] = {
  {4, 1}, {3, 2}, {2, 2}, {1, 3}, {0, 7}, {3, 3}, {3, 2}, {2, 2}, {2, 1},
};

static const ChargingIconRow s_charging_icon_rows_large[] = {
  {5, 1}, {4, 2}, {3, 3}, {2, 3}, {1, 3}, {0, 9}, {0, 9}, {4, 3}, {3, 3}, {3, 2}, {2, 2}, {2, 1},
};

#define CHARGING_ICON_SMALL_WIDTH 7
#define CHARGING_ICON_LARGE_WIDTH 9

typedef struct LauncherAppGlanceSettingsState {
  BatteryChargeState battery_charge_state;
  bool is_pebble_app_connected;
  bool is_airplane_mode_enabled;
  bool is_quiet_time_enabled;
#ifdef CONFIG_SERVICE_BLE_HRM
  bool is_sharing_hrm;
#endif
} LauncherAppGlanceSettingsState;

typedef struct LauncherAppGlanceSettings {
  char title[APP_NAME_SIZE_BYTES];
  char battery_percent_text[5]; //!< longest string is "100%" (4 characters + 1 for NULL terminator)
  KinoReel *icon;
  uint32_t icon_resource_id;
  uint8_t subtitle_font_height;
  LauncherAppGlanceSettingsState glance_state;
  EventServiceInfo battery_state_event_info;
  EventServiceInfo pebble_app_event_info;
  EventServiceInfo airplane_mode_event_info;
  EventServiceInfo quiet_time_event_info;
#ifdef CONFIG_SERVICE_BLE_HRM
  EventServiceInfo hrm_sharing_event_info;
#endif
} LauncherAppGlanceSettings;

static KinoReel *prv_get_icon(LauncherAppGlanceStructured *structured_glance) {
  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);
  return NULL_SAFE_FIELD_ACCESS(settings_glance, icon, nullptr);
}

static const char *prv_get_title(LauncherAppGlanceStructured *structured_glance) {
  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);
  return NULL_SAFE_FIELD_ACCESS(settings_glance, title, nullptr);
}

//! Returns where the subtitle font draws its digits, relative to the top of a subtitle line
static GRect prv_get_digit_frame(GContext *ctx, LauncherAppGlanceStructured *structured_glance) {
  GFont font = structured_glance->subtitle_font;
  const GRect glyph = text_resources_get_glyph_frame(&ctx->font_cache, '0', font);
  return GRect(0, glyph.origin.y - fonts_get_font_cap_offset(font), 0, glyph.size.h);
}

static void prv_charging_icon_node_draw_cb(GContext *ctx, const GRect *rect,
                                           [[maybe_unused]] const GTextNodeDrawConfig *config,
                                           bool render, GSize *size_out, void *user_data) {
  LauncherAppGlanceStructured *structured_glance = user_data;
  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);

  // Stretch the closest bolt to the digit height
  const GRect digits = prv_get_digit_frame(ctx, structured_glance);
  const int16_t h = digits.size.h;
  const bool is_large = (h >= (int16_t)ARRAY_LENGTH(s_charging_icon_rows_large));
  const ChargingIconRow *rows = is_large ? s_charging_icon_rows_large : s_charging_icon_rows_small;
  const int16_t num_rows = is_large ? ARRAY_LENGTH(s_charging_icon_rows_large)
                                    : ARRAY_LENGTH(s_charging_icon_rows_small);
  const int16_t width =
      (is_large ? CHARGING_ICON_LARGE_WIDTH : CHARGING_ICON_SMALL_WIDTH) * h / num_rows;

  if (render) {
    graphics_context_set_fill_color(
        ctx, launcher_app_glance_structured_get_highlight_color(structured_glance));
    for (int16_t y = 0; y < h; y++) {
      const ChargingIconRow *row = &rows[y * num_rows / h];
      const int16_t x = row->x * h / num_rows;
      const int16_t w = MAX(1, (row->x + row->w) * h / num_rows - x);
      graphics_fill_rect(ctx,
                         &GRect(rect->origin.x + x, rect->origin.y + digits.origin.y + y, w, 1));
    }
  }

  if (size_out) {
    *size_out = GSize(width, settings_glance->subtitle_font_height);
  }
}

static void prv_battery_icon_node_draw_cb(GContext *ctx, const GRect *rect,
                                          [[maybe_unused]] const GTextNodeDrawConfig *config,
                                          bool render, GSize *size_out, void *user_data) {
  LauncherAppGlanceStructured *structured_glance = user_data;
  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);

  // Match the digit height, keeping the proportions of the original 16x9 battery
  const GRect digits = prv_get_digit_frame(ctx, structured_glance);
  const int16_t h = digits.size.h;
  const int16_t w = h * 16 / 9;
  const int16_t border = MAX(2, h * 2 / 9);
  const int16_t tip_w = border;

  if (render) {
    const GPoint origin = GPoint(rect->origin.x, rect->origin.y + digits.origin.y);
    const GPoint battery_silhouette_path_points[] = {
      {0, 0},
      {w - 1, 0},
      {w - 1, border - 1},
      {w - 1 + tip_w, border},
      {w - 1 + tip_w, h - 1 - border},
      {w - 1, h - 1 - border},
      {w - 1, h - 1},
      {0, h - 1},
    };
    GPath battery_silhouette_path = (GPath){
      .num_points = ARRAY_LENGTH(battery_silhouette_path_points),
      .points = (GPoint *)battery_silhouette_path_points,
      .offset = origin,
    };

    const GColor battery_silhouette_color =
        launcher_app_glance_structured_get_highlight_color(structured_glance);
    const GColor battery_fill_color =
        PBL_IF_COLOR_ELSE(gcolor_legible_over(battery_silhouette_color), GColorWhite);

    graphics_context_set_fill_color(ctx, battery_silhouette_color);

    // Draw the battery silhouette
    const GRect battery_silhouette_frame = (GRect){
      .origin = origin,
      .size = GSize(w, h),
    };
    gpath_draw_filled(ctx, &battery_silhouette_path);

    // Inset the filled area
    GRect battery_fill_rect = grect_inset_internal(battery_silhouette_frame, border + 1, border);
#if !PBL_COLOR
    // Fill the battery silhouette all the way for B&W, in order to make the BG black always.
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, &battery_fill_rect);
#endif

    // Adjust fill width for charge percentage, never filling below 10%
    uint8_t clipped_charge_percent =
        settings_glance->glance_state.battery_charge_state.charge_percent;
    clipped_charge_percent = CLIP(clipped_charge_percent, (uint8_t)10, (uint8_t)100);
    battery_fill_rect.size.w = battery_fill_rect.size.w * clipped_charge_percent / (int16_t)100;
    // Fill the battery silhouette based on the charge percent
    graphics_context_set_fill_color(ctx, battery_fill_color);
    graphics_fill_rect(ctx, &battery_fill_rect);
  }

  if (size_out) {
    *size_out = GSize(w, settings_glance->subtitle_font_height);
  }
}

static void prv_battery_percent_dynamic_text_node_update(
    [[maybe_unused]] GContext *ctx, [[maybe_unused]] GTextNode *node,
    [[maybe_unused]] const GRect *box, [[maybe_unused]] const GTextNodeDrawConfig *config,
    [[maybe_unused]] bool render, char *buffer, size_t buffer_size, void *user_data) {
  LauncherAppGlanceStructured *structured_glance = user_data;
  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);
  if (settings_glance) {
    buffer_size = MIN(sizeof(settings_glance->battery_percent_text), buffer_size);
    strncpy(buffer, settings_glance->battery_percent_text, buffer_size);
    buffer[buffer_size - 1] = '\0';
  }
}

static GTextNode *prv_wrap_text_node_in_vertically_centered_container(GTextNode *node) {
  const size_t max_vertical_container_nodes = 1;
  GTextNodeVertical *vertical_container_node =
      graphics_text_node_create_vertical(max_vertical_container_nodes);
  vertical_container_node->vertical_alignment = GVerticalAlignmentCenter;

  graphics_text_node_container_add_child(&vertical_container_node->container, node);

  return &vertical_container_node->container.node;
}

static GTextNode *prv_create_subtitle_node(LauncherAppGlanceStructured *structured_glance) {
  PBL_ASSERTN(structured_glance);
  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);
  PBL_ASSERTN(settings_glance);

  // Battery text (if not plugged in), battery icon, and (if plugged in) a lightning bolt icon
  const size_t max_horizontal_nodes = 3;
  GTextNodeHorizontal *horizontal_container_node =
      graphics_text_node_create_horizontal(max_horizontal_nodes);
  horizontal_container_node->horizontal_alignment = GTextAlignmentLeft;

  if (!settings_glance->glance_state.battery_charge_state.is_plugged) {
    GTextNode *battery_percent_text_node = launcher_app_glance_structured_create_subtitle_text_node(
        structured_glance, prv_battery_percent_dynamic_text_node_update);
    // Achieves the design spec'd 6 px horizontal spacing b/w the percent text and battery icon
    battery_percent_text_node->margin.w = 4;
    GTextNode *vertically_centered_battery_percent_text_node =
        prv_wrap_text_node_in_vertically_centered_container(battery_percent_text_node);
    graphics_text_node_container_add_child(&horizontal_container_node->container,
                                           vertically_centered_battery_percent_text_node);
  }

  GTextNodeCustom *battery_icon_node =
      graphics_text_node_create_custom(prv_battery_icon_node_draw_cb, structured_glance);

  // Achieves the design spec'd 6 px horizontal spacing b/w the battery icon and charging icon
  battery_icon_node->node.margin.w = 7;
  GTextNode *vertically_centered_battery_icon_node =
      prv_wrap_text_node_in_vertically_centered_container(&battery_icon_node->node);
  graphics_text_node_container_add_child(&horizontal_container_node->container,
                                         vertically_centered_battery_icon_node);

  if (settings_glance->glance_state.battery_charge_state.is_plugged) {
    GTextNodeCustom *charging_icon_node =
        graphics_text_node_create_custom(prv_charging_icon_node_draw_cb, structured_glance);
    GTextNode *vertically_centered_charging_icon_node =
        prv_wrap_text_node_in_vertically_centered_container(&charging_icon_node->node);
    graphics_text_node_container_add_child(&horizontal_container_node->container,
                                           vertically_centered_charging_icon_node);
  }

  return &horizontal_container_node->container.node;
}

static void prv_destructor(LauncherAppGlanceStructured *structured_glance) {
  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);
  if (settings_glance) {
    event_service_client_unsubscribe(&settings_glance->battery_state_event_info);
    event_service_client_unsubscribe(&settings_glance->pebble_app_event_info);
    event_service_client_unsubscribe(&settings_glance->airplane_mode_event_info);
    event_service_client_unsubscribe(&settings_glance->quiet_time_event_info);
#ifdef CONFIG_SERVICE_BLE_HRM
    event_service_client_unsubscribe(&settings_glance->hrm_sharing_event_info);
#endif
    kino_reel_destroy(settings_glance->icon);
  }
  app_free(settings_glance);
}

static void prv_set_glance_icon(LauncherAppGlanceSettings *settings_glance,
                                uint32_t new_icon_resource_id) {
  if (settings_glance->icon_resource_id == new_icon_resource_id) {
    // Nothing to do, bail out
    return;
  }

  // Destroy the existing icon
  kino_reel_destroy(settings_glance->icon);

  // Set the new icon and record its resource ID
  settings_glance->icon = kino_reel_create_with_resource(new_icon_resource_id);
  PBL_ASSERTN(settings_glance->icon);
  settings_glance->icon_resource_id = new_icon_resource_id;
}

static bool prv_mute_notifications_allow_calls_only(void) {
  return (alerts_get_mask() == AlertMaskPhoneCalls);
}

static uint32_t prv_get_resource_id_for_connectivity_status(
    LauncherAppGlanceSettings *settings_glance) {
#ifdef CONFIG_SERVICE_BLE_HRM
  if (settings_glance->glance_state.is_sharing_hrm) {
    return RESOURCE_ID_CONNECTIVITY_SHARING_HRM;
  }
#endif
  if (settings_glance->glance_state.is_airplane_mode_enabled) {
    return RESOURCE_ID_CONNECTIVITY_BLUETOOTH_AIRPLANE_MODE;
  } else if (!settings_glance->glance_state.is_pebble_app_connected) {
    return RESOURCE_ID_CONNECTIVITY_BLUETOOTH_DISCONNECTED;
  } else if (settings_glance->glance_state.is_quiet_time_enabled) {
    return RESOURCE_ID_CONNECTIVITY_BLUETOOTH_DND;
  } else if (prv_mute_notifications_allow_calls_only()) {
    return RESOURCE_ID_CONNECTIVITY_BLUETOOTH_CALLS_ONLY;
  } else if (settings_glance->glance_state.is_pebble_app_connected) {
    return RESOURCE_ID_CONNECTIVITY_BLUETOOTH_CONNECTED;
  } else {
    pbl_panic();
  }
}

static void prv_refresh_glance_content(LauncherAppGlanceSettings *settings_glance) {
  // Update the battery percent text in the glance
  const size_t battery_percent_text_size = sizeof(settings_glance->battery_percent_text);
  snprintf(settings_glance->battery_percent_text, battery_percent_text_size, "%" PRIu8 "%%",
           settings_glance->glance_state.battery_charge_state.charge_percent);

  // Update the icon
  const uint32_t new_icon_resource_id =
      prv_get_resource_id_for_connectivity_status(settings_glance);
  prv_set_glance_icon(settings_glance, new_icon_resource_id);
}

static bool prv_is_pebble_app_connected(void) {
  return (comm_session_get_system_session() != nullptr);
}

static void prv_event_handler(PebbleEvent *event, void *context) {
  LauncherAppGlanceStructured *structured_glance = context;
  PBL_ASSERTN(structured_glance);

  LauncherAppGlanceSettings *settings_glance =
      launcher_app_glance_structured_get_data(structured_glance);
  PBL_ASSERTN(settings_glance);

  switch (event->type) {
    case PEBBLE_BATTERY_STATE_CHANGE_EVENT:
      settings_glance->glance_state.battery_charge_state = battery_state_service_peek();
      break;
    case PEBBLE_COMM_SESSION_EVENT:
      if (event->bluetooth.comm_session_event.is_system) {
        settings_glance->glance_state.is_pebble_app_connected =
            event->bluetooth.comm_session_event.is_open;
      }
      break;
    case PBL_BT_PEBBLE_STATE_EVENT:
      settings_glance->glance_state.is_airplane_mode_enabled = bt_ctl_is_airplane_mode_on();
      break;
    case PEBBLE_DO_NOT_DISTURB_EVENT:
      settings_glance->glance_state.is_quiet_time_enabled = do_not_disturb_is_active();
      break;
#ifdef CONFIG_SERVICE_BLE_HRM
    case PEBBLE_BLE_HRM_SHARING_STATE_UPDATED_EVENT: {
      const bool prev_is_sharing = settings_glance->glance_state.is_sharing_hrm;
      const bool is_sharing = (event->bluetooth.le.hrm_sharing_state.subscription_count > 0);
      if (prev_is_sharing == is_sharing) {
        return;
      }
      settings_glance->glance_state.is_sharing_hrm = is_sharing;
      break;
    }
#endif
    default:
      pbl_panic();
  }

  // Refresh the content in the glance
  prv_refresh_glance_content(settings_glance);

  // Broadcast to the service that we changed the glance
  launcher_app_glance_structured_notify_service_glance_changed(structured_glance);
}

static void prv_subscribe_to_event(EventServiceInfo *event_service_info, PebbleEventType type,
                                   LauncherAppGlanceStructured *structured_glance) {
  PBL_ASSERTN(event_service_info);

  *event_service_info = (EventServiceInfo){
    .type = type,
    .handler = prv_event_handler,
    .context = structured_glance,
  };

  event_service_client_subscribe(event_service_info);
}

static const LauncherAppGlanceStructuredImpl s_settings_structured_glance_impl = {
  .get_icon = prv_get_icon,
  .get_title = prv_get_title,
  .create_subtitle_node = prv_create_subtitle_node,
  .destructor = prv_destructor,
};

LauncherAppGlance *launcher_app_glance_settings_create(const AppMenuNode *node) {
  PBL_ASSERTN(node);

  LauncherAppGlanceSettings *settings_glance = app_zalloc_check(sizeof(*settings_glance));

  // Copy the name of the Settings app as the title
  const size_t title_size = sizeof(settings_glance->title);
  strncpy(settings_glance->title, node->name, title_size);
  settings_glance->title[title_size - 1] = '\0';

  // Cache the subtitle font height for simplifying layout calculations
  settings_glance->subtitle_font_height = fonts_get_font_height(
      fonts_get_system_font(launcher_menu_layer_get_style()->subtitle_font_key));

  const bool should_consider_slices = false;
  LauncherAppGlanceStructured *structured_glance = launcher_app_glance_structured_create(
      &node->uuid, &s_settings_structured_glance_impl, should_consider_slices, settings_glance);
  PBL_ASSERTN(structured_glance);
  // Disable selection animations for the settings glance
  structured_glance->selection_animation_disabled = true;

  // Set the first state of the glance
  settings_glance->glance_state = (LauncherAppGlanceSettingsState){
    .battery_charge_state = battery_state_service_peek(),
    .is_pebble_app_connected = prv_is_pebble_app_connected(),
    .is_airplane_mode_enabled = bt_ctl_is_airplane_mode_on(),
    .is_quiet_time_enabled = do_not_disturb_is_active(),
#ifdef CONFIG_SERVICE_BLE_HRM
    .is_sharing_hrm = ble_hrm_is_sharing(),
#endif
  };

  // Refresh the glance now that we have set the first state of the glance
  prv_refresh_glance_content(settings_glance);

  // Subscribe to the various events we care about
  prv_subscribe_to_event(&settings_glance->battery_state_event_info,
                         PEBBLE_BATTERY_STATE_CHANGE_EVENT, structured_glance);
  prv_subscribe_to_event(&settings_glance->pebble_app_event_info, PEBBLE_COMM_SESSION_EVENT,
                         structured_glance);
  prv_subscribe_to_event(&settings_glance->airplane_mode_event_info, PBL_BT_PEBBLE_STATE_EVENT,
                         structured_glance);
  prv_subscribe_to_event(&settings_glance->quiet_time_event_info, PEBBLE_DO_NOT_DISTURB_EVENT,
                         structured_glance);
#ifdef CONFIG_SERVICE_BLE_HRM
  prv_subscribe_to_event(&settings_glance->hrm_sharing_event_info,
                         PEBBLE_BLE_HRM_SHARING_STATE_UPDATED_EVENT, structured_glance);
#endif

  return &structured_glance->glance;
}
