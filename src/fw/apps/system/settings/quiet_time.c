/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "quiet_time.h"
#include "menu.h"
#include "window.h"

#include "applib/ui/action_menu_window_private.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/day_picker.h"
#include "applib/ui/menu_layer.h"
#include "applib/ui/time_range_selection_window.h"
#include "kernel/pbl_malloc.h"
#include "popups/health_tracking_ui.h"
#include "pbl/services/clock.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/notifications/alerts_private.h"
#include "pbl/services/notifications/do_not_disturb.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "pbl/services/notifications/alerts_preferences_private.h"
#include "resource/resource_ids.auto.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"
#include "util/string.h"
#include "shell/prefs.h"

#include <stdio.h>

typedef struct {
  SettingsCallbacks callbacks;
} SettingsQuietTimeData;

typedef struct {
  SettingsCallbacks callbacks;
  QuietTimeScheduleConfig schedules[MAX_QUIET_TIME_SCHEDULES];
  int num_schedules;
  char *action_menu_text;
  TimeRangeSelectionWindowData schedule_window;
  ActionMenuConfig action_menu;
  int selected_schedule_index;

  GBitmap plus_icon;
  uint32_t current_plus_icon_resource_id;
  bool can_add_schedule;
  bool show_limit_reached_text;
} SettingsQuietTimeScheduleData;

#ifdef CONFIG_TOUCH
typedef struct {
  SettingsCallbacks callbacks;
} SettingsQuietTimeBacklightData;
#endif

enum QuietTimeItem {
  QuietTimeItemManual,
  QuietTimeItemSmartDnd,
  QuietTimeItemSchedule,
  QuietTimeItemInterruptions,
  QuietTimeItemNotifications,
#ifdef CONFIG_TOUCH
  QuietTimeItemBacklight,
#else
  QuietTimeItemMotionBacklight,
#endif
#ifdef CONFIG_SPEAKER
  QuietTimeItemMuteSpeaker,
#endif
  QuietTimeItem_Count,
};

#ifdef CONFIG_TOUCH
enum QuietTimeBacklightItem {
  QuietTimeBacklightItemMotion,
  QuietTimeBacklightItemTouch,
  QuietTimeBacklightItem_Count,
};
#endif

static void prv_change_days_callback(DayPickerResult result, void *context);

static const AlertMask s_dnd_mask_cycle[] = {
  AlertMaskAllOff,
  AlertMaskPhoneCalls,
};

static AlertMask prv_cycle_dnd_mask(void) {
  AlertMask mask = alerts_get_dnd_mask();
  int index = 0;
  for (size_t i = 0; i < ARRAY_LENGTH(s_dnd_mask_cycle); i++) {
    if (s_dnd_mask_cycle[i] == mask) {
      index = i;
      break;
    }
  }
  mask = s_dnd_mask_cycle[(index + 1) % ARRAY_LENGTH(s_dnd_mask_cycle)];
  alerts_set_dnd_mask(mask);
  return mask;
}

static const char *prv_get_dnd_mask_subtitle(void *i18n_key) {
  const char *title = NULL;
  switch (alerts_get_dnd_mask()) {
    case AlertMaskAllOff:
      title = i18n_get("Quiet All Notifications", i18n_key);
      break;
    case AlertMaskPhoneCalls:
      title = i18n_get("Allow Phone Calls", i18n_key);
      break;
    default:
      title = "???";
      break;
  }
  return title;
}

static const DndNotificationMode s_dnd_notification_mode_cycle[] = {
  DndNotificationModeShow,
  DndNotificationModeHide,
};

static DndNotificationMode prv_cycle_dnd_notification_mode(void) {
  DndNotificationMode mode = alerts_preferences_dnd_get_show_notifications();
  int index = 0;
  for (size_t i = 0; i < ARRAY_LENGTH(s_dnd_notification_mode_cycle); i++) {
    if (s_dnd_notification_mode_cycle[i] == mode) {
      index = i;
      break;
    }
  }
  mode = s_dnd_notification_mode_cycle[(index + 1) % ARRAY_LENGTH(s_dnd_notification_mode_cycle)];
  alerts_preferences_dnd_set_show_notifications(mode);
  return mode;
}

static const char *prv_get_dnd_notifications_enable(void *i18n_key) {
  switch (alerts_preferences_dnd_get_show_notifications()) {
    case DndNotificationModeShow:
      return i18n_get("Show", i18n_key);
    case DndNotificationModeHide:
      return i18n_get("Hide", i18n_key);
    default:
      return "???";
  }
}

static const char *prv_get_smart_dnd_subtitle(void *i18n_key) {
  return i18n_get(do_not_disturb_is_smart_dnd_enabled() ? "On" : "Off", i18n_key);
}

static void prv_get_qt_time(const QuietTimeScheduleConfig *config, char *time_string,
                             const uint8_t len) {
  clock_format_time(time_string, len, config->from_hour, config->from_minute, true);
  strcat(time_string, " - ");
  uint8_t current_length = strnlen(time_string, len);
  char *buffer = time_string + current_length;
  clock_format_time(buffer, len - current_length, config->to_hour, config->to_minute, true);
}

static void prv_reload_schedules(SettingsQuietTimeScheduleData *data) {
  data->num_schedules = 0;
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    quiet_time_get_schedule(i, &data->schedules[i]);
    if (data->schedules[i].is_used) {
      data->num_schedules++;
    }
  }
  data->can_add_schedule = (data->num_schedules < MAX_QUIET_TIME_SCHEDULES);
}

static void prv_schedule_refresh(SettingsQuietTimeScheduleData *data) {
  prv_reload_schedules(data);
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! DND Action Menu Window
///////////////////////////////////////////////////////////////////////////////////////////////////

enum {
  DNDMenuItemDisable = 0,
  DNDMenuItemChangeSchedule,
  DNDMenuItemChangeDays,
  DNDMenuItemDelete,
  DNDMenuItem_Count
};

static void prv_toggle_scheduled_dnd(ActionMenu *action_menu,
                                     const ActionMenuItem *item,
                                     void *context) {
  int index = (int)(uintptr_t)item->action_data;
  SettingsQuietTimeScheduleData *data = context;
  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);
  config.enabled = !config.enabled;
  quiet_time_set_schedule_enabled(index, config.enabled);
  prv_schedule_refresh(data);
}

static void prv_complete_schedule(TimeRangeSelectionWindowData *schedule_window, void *data) {
  SettingsQuietTimeScheduleData *qt_data = (SettingsQuietTimeScheduleData *)data;
  int index = qt_data->selected_schedule_index;
  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);

  config.from_hour = schedule_window->from.hour;
  config.from_minute = schedule_window->from.minute;
  config.to_hour = schedule_window->to.hour;
  config.to_minute = schedule_window->to.minute;

  if (config.from_hour == config.to_hour && config.from_minute == config.to_minute) {
    if ((config.to_minute = (config.to_minute + 1) % 60) == 0) {
      config.to_hour = (config.to_hour + 1) % 24;
    }
  }

  quiet_time_set_schedule(index, &config);

  prv_schedule_refresh(qt_data);

  const bool animated = true;
  app_window_stack_remove(&schedule_window->window, animated);
}

static void prv_time_range_select_window_push(int index,
                                              SettingsQuietTimeScheduleData *data) {
  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);
  TimeRangeSelectionWindowData *schedule_window = &data->schedule_window;
  data->selected_schedule_index = index;
  time_range_selection_window_init(schedule_window, GColorCobaltBlue,
                                   prv_complete_schedule, data);

  schedule_window->from.hour = config.from_hour;
  schedule_window->from.minute = config.from_minute;
  schedule_window->to.hour = config.to_hour;
  schedule_window->to.minute = config.to_minute;
  app_window_stack_push(&schedule_window->window, true);
}

static void prv_dnd_set_schedule(ActionMenu *action_menu,
                                const ActionMenuItem *item,
                                void *context) {
  int index = (int)(uintptr_t)item->action_data;
  SettingsQuietTimeScheduleData *data = context;
  quiet_time_set_schedule_enabled(index, true);
  data->selected_schedule_index = index;
  prv_schedule_refresh(data);
  prv_time_range_select_window_push(index, data);
}

static void prv_dnd_delete_schedule(ActionMenu *action_menu,
                                     const ActionMenuItem *item,
                                     void *context) {
  int index = (int)(uintptr_t)item->action_data;
  SettingsQuietTimeScheduleData *data = context;
  quiet_time_delete_schedule(index);
  prv_schedule_refresh(data);
}

static void prv_dnd_change_days(ActionMenu *action_menu,
                                const ActionMenuItem *item,
                                void *context) {
  int index = (int)(uintptr_t)item->action_data;
  SettingsQuietTimeScheduleData *data = context;
  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);

  DayPickerResult initial;
  initial.kind = (DayPickerKind)config.kind;
  memcpy(initial.custom_days, config.scheduled_days, sizeof(initial.custom_days));
  DayPickerConfig picker_config = {
    .initial = initial,
    .highlight_color = shell_prefs_get_theme_highlight_color(),
    .allow_once = false,
  };
  day_picker_push(picker_config, prv_change_days_callback, data);
}

static void prv_scheduled_dnd_menu_cleanup(ActionMenu *action_menu,
                                  const ActionMenuItem *item,
                                  void *context) {
  ActionMenuLevel *root_level = action_menu_get_root_level(action_menu);
  SettingsQuietTimeScheduleData *data = context;
  time_range_selection_window_deinit(&data->schedule_window);
  app_free(data->action_menu_text);
  i18n_free_all(&data->action_menu);
  task_free(root_level);
}

static void prv_scheduled_dnd_menu_push(int index,
                                        SettingsQuietTimeScheduleData *data) {
  data->action_menu = (ActionMenuConfig) {
    .context = data,
    .colors.background = shell_prefs_get_theme_highlight_color(),
    .did_close = prv_scheduled_dnd_menu_cleanup,
  };

  ActionMenuLevel *level =
      task_malloc_check(sizeof(ActionMenuLevel) + DNDMenuItem_Count * sizeof(ActionMenuItem));
  *level = (ActionMenuLevel) {
    .num_items = DNDMenuItem_Count,
    .display_mode = ActionMenuLevelDisplayModeWide,
  };

  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);
  const uint8_t text_max_size = 30;
  const uint8_t buffer_size = text_max_size + 22;
  data->action_menu_text = app_malloc_check(buffer_size);

  if (config.enabled) {
    strncpy(data->action_menu_text, i18n_get("Disable", &data->action_menu), buffer_size);
  } else {
    strncpy(data->action_menu_text, i18n_get("Enable", &data->action_menu), text_max_size);
    strcat(data->action_menu_text, " (");
    uint8_t current_length = strnlen(data->action_menu_text, buffer_size);
    char *buffer = data->action_menu_text + current_length;
    prv_get_qt_time(&config, buffer, buffer_size - current_length);
    strcat(data->action_menu_text, ")");
  }

  level->items[DNDMenuItemDisable] = (ActionMenuItem) {
    .label = data->action_menu_text,
    .perform_action = prv_toggle_scheduled_dnd,
    .action_data = (void*)(uintptr_t)index,
  };

  level->items[DNDMenuItemChangeSchedule] = (ActionMenuItem) {
    .label = i18n_get("Change Time", &data->action_menu),
    .perform_action = prv_dnd_set_schedule,
    .action_data = (void*)(uintptr_t)index,
  };

  level->items[DNDMenuItemChangeDays] = (ActionMenuItem) {
    .label = i18n_get("Change Days", &data->action_menu),
    .perform_action = prv_dnd_change_days,
    .action_data = (void*)(uintptr_t)index,
  };

  level->items[DNDMenuItemDelete] = (ActionMenuItem) {
    .label = i18n_get("Delete", &data->action_menu),
    .perform_action = prv_dnd_delete_schedule,
    .action_data = (void*)(uintptr_t)index,
  };

  data->action_menu.root_level = level;
  app_action_menu_open(&data->action_menu);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Day picker callback for adding new schedule
///////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_add_day_picker_callback(DayPickerResult result, void *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *)context;
  QuietTimeScheduleConfig config = {
    .kind = (QuietTimeKind)result.kind,
    .enabled = true,
    .from_hour = 22,
    .from_minute = 0,
    .to_hour = 7,
    .to_minute = 0,
  };
  memcpy(config.scheduled_days, result.custom_days, sizeof(config.scheduled_days));

  int index = quiet_time_create_schedule(&config);
  if (index >= 0) {
    quiet_time_set_schedule_enabled(index, true);
    prv_schedule_refresh(data);
    prv_time_range_select_window_push(index, data);
  }
}

static void prv_change_days_callback(DayPickerResult result, void *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *)context;
  int index = data->selected_schedule_index;

  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);
  config.kind = (QuietTimeKind)result.kind;
  memcpy(config.scheduled_days, result.custom_days, sizeof(config.scheduled_days));

  if (config.kind != QT_KIND_CUSTOM) {
    memset(config.scheduled_days, 0, sizeof(config.scheduled_days));
  }
  quiet_time_set_schedule(index, &config);
  prv_schedule_refresh(data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Schedule sub-menu
///////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_schedule_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;
  gbitmap_deinit(&data->plus_icon);
  i18n_free_all(data);
  app_free(data);
}

static void prv_schedule_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                     const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;

  // Row 0: the "+ Add Schedule" cell, mirroring the Alarms list.
  if (row == 0) {
    GRect box;
    uint32_t new_bitmap_resource = RESOURCE_ID_PLUS_ICON_BLACK;

    if (!data->can_add_schedule) { // schedule limit reached
      if (menu_cell_layer_is_highlighted(cell_layer)) {
        if (data->show_limit_reached_text) {
          const GFont font =
              system_theme_get_font_for_default_size(TextStyleFont_MenuCellSubtitle);

          box = GRect(0, 0, cell_layer->bounds.size.w, fonts_get_font_height(font));

          const char *text = i18n_get("Limit reached.", data);
          box.size = graphics_text_layout_get_max_used_size(ctx, text, font, box,
                                                            GTextOverflowModeTrailingEllipsis,
                                                            GTextAlignmentCenter, NULL);
          grect_align(&box, &cell_layer->bounds, GAlignCenter, true /* clip */);
          box.origin.y -= fonts_get_font_cap_offset(font);

          graphics_draw_text(ctx, text, font, box,
                             GTextOverflowModeFill, GTextAlignmentCenter, NULL);
          return;
        } else { // "+" cell highlighted
          new_bitmap_resource = RESOURCE_ID_PLUS_ICON_DOTTED;
        }
      } else { // "+" cell not highlighted
        graphics_context_set_tint_color(ctx, GColorLightGray);
      }
    }

    if (new_bitmap_resource != data->current_plus_icon_resource_id) {
      data->current_plus_icon_resource_id = new_bitmap_resource;
      gbitmap_deinit(&data->plus_icon);
      gbitmap_init_with_resource(&data->plus_icon, data->current_plus_icon_resource_id);
    }

    box.origin = GPoint((cell_layer->bounds.size.w - data->plus_icon.bounds.size.w) / 2,
                        (cell_layer->bounds.size.h - data->plus_icon.bounds.size.h) / 2);
    box.size = data->plus_icon.bounds.size;
    graphics_context_set_compositing_mode(ctx, GCompOpTint);
    graphics_draw_bitmap_in_rect(ctx, &data->plus_icon, &box);
    return;
  }

  // Rows 1..N: each scheduled quiet-time entry, alarm-style.
  int idx = row - 1;
  QuietTimeScheduleConfig *config = &data->schedules[idx];

  // Title: time range, e.g. "10:00 PM - 6:00 AM".
  const uint8_t buffer_length = 32;
  char title_buf[buffer_length];
  prv_get_qt_time(config, title_buf, buffer_length);

  // Subtitle: kind ("Weekday", "Weekend") or comma-separated custom day list.
  const size_t day_text_length = 32;
  char day_text[day_text_length];
  memset(day_text, 0, sizeof(day_text));
  const char *subtitle;
  if (config->kind == QT_KIND_CUSTOM) {
    quiet_time_get_string_for_custom(config->scheduled_days, day_text, sizeof(day_text));
    subtitle = day_text;
  } else {
    subtitle = i18n_get(quiet_time_get_string_for_kind(config->kind), data);
  }

  // Value: ON / OFF, right-aligned.
  const char *value = config->enabled ? i18n_get("ON", data) : i18n_get("OFF", data);

  MenuCellLayerConfig cell_config = {
    .title = title_buf,
    .subtitle = subtitle,
    .value = value,
    .overflow_mode = GTextOverflowModeTrailingEllipsis,
    .horizontal_inset = PBL_IF_ROUND_ELSE(-6, 0),
  };
  menu_cell_layer_draw(ctx, cell_layer, &cell_config);
}

static void prv_schedule_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;

  if (row == 0) {
    if (!data->can_add_schedule) {
      data->show_limit_reached_text = true;
      settings_menu_reload_data(SettingsMenuItemQuietTime);
      return;
    }
    DayPickerResult initial = {.kind = DayPickerKindEveryday};
    memset(initial.custom_days, 0, sizeof(initial.custom_days));
    DayPickerConfig config = {
      .initial = initial,
      .highlight_color = shell_prefs_get_theme_highlight_color(),
      .allow_once = false,
    };
    day_picker_push(config, prv_add_day_picker_callback, data);
  } else {
    int idx = row - 1;
    data->selected_schedule_index = idx;
    prv_scheduled_dnd_menu_push(idx, data);
  }
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static uint16_t prv_schedule_num_rows_cb(SettingsCallbacks *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;
  // Row 0 is the "+" cell, then one row per active schedule.
  return 1 + data->num_schedules;
}

static void prv_schedule_submenu_push(void) {
  SettingsQuietTimeScheduleData *data = app_zalloc_check(sizeof(*data));

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_schedule_deinit_cb,
    .draw_row = prv_schedule_draw_row_cb,
    .select_click = prv_schedule_select_click_cb,
    .num_rows = prv_schedule_num_rows_cb,
  };

  prv_reload_schedules(data);

  Window *window = settings_window_create_with_title(SettingsMenuItemQuietTime,
                                                     i18n_noop("Schedule"), &data->callbacks);
  app_window_stack_push(window, true /* animated */);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Backlight sub-menu (touch boards)
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_TOUCH
static void prv_backlight_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeBacklightData *data = (SettingsQuietTimeBacklightData *) context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_backlight_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                      const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsQuietTimeBacklightData *data = (SettingsQuietTimeBacklightData *) context;
  const char *title = NULL;
  const char *subtitle = NULL;
  switch (row) {
    case QuietTimeBacklightItemMotion:
      title = i18n_noop("Motion");
      subtitle = alerts_preferences_dnd_get_motion_backlight() ? i18n_noop("On") : i18n_noop("Off");
      break;
    case QuietTimeBacklightItemTouch:
      title = i18n_noop("Touch");
      subtitle = alerts_preferences_dnd_get_touch_backlight() ? i18n_noop("On") : i18n_noop("Off");
      break;
    default:
        WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static void prv_backlight_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  switch (row) {
    case QuietTimeBacklightItemMotion:
      alerts_preferences_dnd_set_motion_backlight(!alerts_preferences_dnd_get_motion_backlight());
      break;
    case QuietTimeBacklightItemTouch:
      alerts_preferences_dnd_set_touch_backlight(!alerts_preferences_dnd_get_touch_backlight());
      break;
    default:
      WTF;
  }
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static uint16_t prv_backlight_num_rows_cb(SettingsCallbacks *context) {
  return QuietTimeBacklightItem_Count;
}

static void prv_backlight_submenu_push(void) {
  SettingsQuietTimeBacklightData *data = app_zalloc_check(sizeof(*data));

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_backlight_deinit_cb,
    .draw_row = prv_backlight_draw_row_cb,
    .select_click = prv_backlight_select_click_cb,
    .num_rows = prv_backlight_num_rows_cb,
  };

  Window *window = settings_window_create_with_title(SettingsMenuItemQuietTime,
                                                     i18n_noop("Backlight"), &data->callbacks);
  app_window_stack_push(window, true /* animated */);
}
#endif  // CONFIG_TOUCH

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Top-level Quiet Time menu
///////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeData *data = (SettingsQuietTimeData *) context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsQuietTimeData *data = (SettingsQuietTimeData *) context;
  const char *title = NULL;
  const char *subtitle = NULL;

  switch (row) {
    case QuietTimeItemManual:
      title = i18n_get("Manual", data);
      subtitle = do_not_disturb_is_manually_enabled() ?
                     i18n_get("On", data) : i18n_get("Off", data);
      break;
    case QuietTimeItemSmartDnd:
      title = i18n_get("Calendar Aware", data);
      subtitle = prv_get_smart_dnd_subtitle(data);
      break;
    case QuietTimeItemSchedule:
      title = i18n_get("Schedule", data);
      break;
    case QuietTimeItemInterruptions:
      title = i18n_get("Interruptions", data);
      subtitle = prv_get_dnd_mask_subtitle(data);
      break;
    case QuietTimeItemNotifications:
      title = i18n_get("Notifications", data);
      subtitle = prv_get_dnd_notifications_enable(data);
      break;
#ifdef CONFIG_TOUCH
    case QuietTimeItemBacklight:
      title = i18n_get("Backlight", data);
      break;
#else
    case QuietTimeItemMotionBacklight:
      title = i18n_get("Motion Backlight", data);
      subtitle = alerts_preferences_dnd_get_motion_backlight() ?
                     i18n_get("On", data) : i18n_get("Off", data);
      break;
#endif
#ifdef CONFIG_SPEAKER
    case QuietTimeItemMuteSpeaker:
      title = i18n_get("Mute Speaker", data);
      subtitle = alerts_preferences_dnd_get_mute_speaker() ?
                     i18n_get("On", data) : i18n_get("Off", data);
      break;
#endif
    default:
        WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  switch (row) {
    case QuietTimeItemManual:
      do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSourceSettingsMenu);
      break;
    case QuietTimeItemSmartDnd:
      do_not_disturb_toggle_smart_dnd();
      break;
    case QuietTimeItemSchedule:
      prv_schedule_submenu_push();
      break;
    case QuietTimeItemInterruptions:
      prv_cycle_dnd_mask();
      break;
    case QuietTimeItemNotifications:
      prv_cycle_dnd_notification_mode();
      break;
#ifdef CONFIG_TOUCH
    case QuietTimeItemBacklight:
      prv_backlight_submenu_push();
      break;
#else
    case QuietTimeItemMotionBacklight:
      alerts_preferences_dnd_set_motion_backlight(!alerts_preferences_dnd_get_motion_backlight());
      break;
#endif
#ifdef CONFIG_SPEAKER
    case QuietTimeItemMuteSpeaker:
      alerts_preferences_dnd_set_mute_speaker(!alerts_preferences_dnd_get_mute_speaker());
      break;
#endif
    default:
        WTF;
  }
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return QuietTimeItem_Count;
}

static Window *prv_init(void) {
  SettingsQuietTimeData* data = app_zalloc_check(sizeof(*data));

  data->callbacks = (SettingsCallbacks) {
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
  };

  return settings_window_create(SettingsMenuItemQuietTime, &data->callbacks);
}

const SettingsModuleMetadata *settings_quiet_time_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Quiet Time"),
    .init = prv_init,
  };

  return &s_module_info;
}
