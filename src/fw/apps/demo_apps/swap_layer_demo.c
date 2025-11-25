/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "swap_layer_demo.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/status_bar_layer.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "applib/ui/window_stack.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "popups/phone_ui.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "services/normal/timeline/notification_layout.h"
#include "services/normal/timeline/swap_layer.h"
#include "services/normal/timeline/timeline_resources.h"
#include "shell/normal/watchface.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"

#include <string.h>

#define MINUTES(m) ((m) * SECONDS_PER_MINUTE)
#define HOURS(m) ((m) * SECONDS_PER_MINUTE * MINUTES_PER_HOUR)

typedef struct {
  TimelineItemType type;
  LayoutId layout_id;
  uint32_t icon_id;
  uint8_t bg_color;
  uint8_t prim_color;
  uint8_t sec_color;
  char *title;
  char *subtitle;
  char *location;
  char *body;
  int32_t time_offset;
} TestNotification;

static const TestNotification notifications[] = {
  {
    .type = TimelineItemTypeNotification,
    .layout_id = LayoutIdNotification,
    .icon_id = TIMELINE_RESOURCE_NOTIFICATION_GOOGLE_HANGOUTS,
    .bg_color = GColorJaegerGreenARGB8,
    .title = "Henry Levak",
    .body = "Welcome mighty Irken soldiers! "
      "You are the finest examples of military training the Irken army has to offer! "
      "Good for you. Standing behind us, however, are the soldiers we've chosen for roles "
      "in one of the most crucial parts in Operation Impending Doom II! "
      "[mockingly] You in the audience just get to sit and watch.",
    .time_offset = -MINUTES(5)
  },
  {
    .type = TimelineItemTypeNotification,
    .layout_id = LayoutIdNotification,
    .icon_id = TIMELINE_RESOURCE_GENERIC_EMAIL,
    .bg_color = GColorVividCeruleanARGB8,
    .title = "Henry Levak",
    .subtitle = "Henry sent you a 1-1 message",
    .body = "What is an alternative",
    .time_offset = -MINUTES(5)
  },
  {
    .type = TimelineItemTypeReminder,
    .layout_id = LayoutIdReminder,
    .icon_id = TIMELINE_RESOURCE_NOTIFICATION_REMINDER,
    .title = "Implementation Design Review",
    .location = "High (Room 12)\nPebble PA Office",
    .body = "with Liron Damir and 10 other people",
    .time_offset = MINUTES(10),
  }
};

#define NUM_NOTIFS ((int)ARRAY_LENGTH(notifications))

typedef struct SwapLayerDemoData {
  Window window;
  StatusBarLayer status_layer;
  SwapLayer swap_layer;
  LayoutLayer *layout_layers[NUM_NOTIFS];
  uint32_t idx;
} SwapLayerDemoData;

static LayoutLayer *prv_get_layout_handler(SwapLayer *swap_layer, int8_t rel_position,
                                           void *context) {
  PBL_LOG(LOG_LEVEL_DEBUG, "getting layer %d", rel_position);
  SwapLayerDemoData *data = context;


  int8_t new_idx = data->idx + rel_position;
  if (0 > new_idx || new_idx >= NUM_NOTIFS) {
    return NULL;
  }

  return data->layout_layers[new_idx];
}

static void prv_layout_removed_handler(SwapLayer *swap_layer, LayoutLayer *layout,
                                       void *context) {
  // layer_destroy(layer);
}

static void prv_layout_will_appear_handler(SwapLayer *swap_layer, LayoutLayer *layout,
                                           void *context) {
}

static void prv_layout_did_appear_handler(SwapLayer *swap_layer, LayoutLayer *layout,
                                         int8_t rel_change, void *context) {
  SwapLayerDemoData *data = context;
  data->idx += rel_change;
}

static void prv_update_colors_handler(SwapLayer *swap_layer, GColor bg_color,
                                      bool status_bar_filled, void *context) {
  SwapLayerDemoData *data = context;

  GColor status_color = PBL_IF_RECT_ELSE((status_bar_filled) ? bg_color : GColorWhite, GColorClear);
  status_bar_layer_set_colors(&data->status_layer, status_color,
                              gcolor_legible_over(status_color));
}

static void prv_show_incoming_call(void *data) {
  PebblePhoneCaller caller = {
    .number = "+55 408-555-1212",
    .name = "Pankajavalli Balamurugan",
  };
  phone_ui_handle_incoming_call(&caller, true, false, PhoneCallSource_PP);
}

static void prv_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  launcher_task_add_callback(prv_show_incoming_call, NULL);
}

static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  SwapLayerDemoData *data = context;
  data->idx = 0;
  swap_layer_reload_data(&data->swap_layer);
}

static void prv_click_config_provider(void *context) {
  SwapLayerDemoData *data = context;
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_single_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_click_handler, NULL);
  window_set_click_context(BUTTON_ID_SELECT, data);
}

///////////////////
// Window callbacks

static void prv_window_load(Window *window) {
  SwapLayerDemoData *data = app_state_get_user_data();
  Layer *root = window_get_root_layer(window);

  data->idx = 0;

  // configure scroll layer
  SwapLayer *swap_layer = &data->swap_layer;
  GRect swap_layer_frame = root->frame;
  swap_layer_frame.origin.y += STATUS_BAR_LAYER_HEIGHT;
  swap_layer_frame.size.h -= STATUS_BAR_LAYER_HEIGHT;
  swap_layer_init(swap_layer, &swap_layer_frame);
  swap_layer_set_callbacks(swap_layer, data, (SwapLayerCallbacks) {
    .get_layout_handler = prv_get_layout_handler,
    .layout_removed_handler = prv_layout_removed_handler,
    .layout_will_appear_handler = prv_layout_will_appear_handler,
    .layout_did_appear_handler = prv_layout_did_appear_handler,
    .update_colors_handler = prv_update_colors_handler,
    .click_config_provider = prv_click_config_provider,
  });
  layer_add_child(root, swap_layer_get_layer(swap_layer));

  // configure status layer
  StatusBarLayer *status_layer = &data->status_layer;
  status_bar_layer_init(status_layer);
  status_bar_layer_set_colors(status_layer, GColorClear, GColorBlack);
  status_bar_layer_set_separator_mode(status_layer, StatusBarLayerSeparatorModeNone);
  layer_add_child(root, (Layer *)status_layer);

  swap_layer_set_click_config_onto_window(swap_layer, window);
}

static void handle_init(void) {
  SwapLayerDemoData *data = app_malloc_check(sizeof(SwapLayerDemoData));

  memset(data, 0, sizeof(SwapLayerDemoData));
  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Swap Layer"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });


  for (int i = 0; i < NUM_NOTIFS; i++) {
    TestNotification notif = notifications[i];

    AttributeList attr_list = {0};
    if (notif.bg_color != GColorClearARGB8) {
      attribute_list_add_uint8(&attr_list, AttributeIdBgColor, notif.bg_color);
    }

    if (notif.sec_color != GColorClearARGB8) {
      attribute_list_add_uint8(&attr_list, AttributeIdSecondaryColor, notif.sec_color);
    }

    if (notif.prim_color != GColorClearARGB8) {
      attribute_list_add_uint8(&attr_list, AttributeIdPrimaryColor, notif.prim_color);
    }

    if (notif.icon_id != 0) {
      attribute_list_add_uint32(&attr_list, AttributeIdIconTiny, notif.icon_id);
    }

    if (notif.title) {
      attribute_list_add_cstring(&attr_list, AttributeIdTitle, notif.title);
    }

    if (notif.subtitle) {
      attribute_list_add_cstring(&attr_list, AttributeIdSubtitle, notif.subtitle);
    }

    if (notif.body) {
      attribute_list_add_cstring(&attr_list, AttributeIdBody, notif.body);
    }

    if (notif.location) {
      attribute_list_add_cstring(&attr_list, AttributeIdLocationName, notif.location);
    }

    uint32_t timestamp = (rtc_get_time() + notif.time_offset);

    TimelineItem *notification =
        timeline_item_create_with_attributes(timestamp, 0, notif.type,
                                             notif.layout_id, &attr_list, NULL);

    const LayoutLayerConfig config = {
      .frame = &window->layer.frame,
      .attributes = &notification->attr_list,
      .mode = LayoutLayerModeCard,
      .app_id = &notification->header.parent_id,
      .context =  &(NotificationLayoutInfo) {
        .item = notification,
#if !PLATFORM_TINTIN
        .show_notification_timestamp = true,
#endif
      },
    };
    LayoutLayer *layout = layout_create(notification->header.layout, &config);

    data->layout_layers[i] = layout;
  }

  const bool animated = true;
  app_window_stack_push(window, animated);
}

////////////////////
// App boilerplate

static void s_main(void) {
  handle_init();

  app_event_loop();
}

const PebbleProcessMd* swap_layer_demo_get_app_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common = {
      .main_func = s_main,
      // UUID: 12a32d95-ef69-46d4-a0b9-854cc62f97f9
      .uuid = {0x12, 0xa3, 0x2d, 0x95, 0xef, 0x69, 0x46, 0xd4,
               0xa0, 0xb9, 0x85, 0x4c, 0xc6, 0x2f, 0x97, 0xf9},
    },
    .name = "SwapLayer Demo",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
