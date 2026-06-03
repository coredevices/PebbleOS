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
} SettingsQuietTimeScheduleData;

#ifdef CONFIG_TOUCH
typedef struct {
  SettingsCallbacks callbacks;
} SettingsQuietTimeBacklightData;
#endif

enum QuietTimeItem {
  QuietTimeItemManual,
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
  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);
  config.enabled = !config.enabled;
  quiet_time_set_schedule_enabled(index, config.enabled);
}

static void prv_complete_schedule(TimeRangeSelectionWindowData *schedule_window, void *data) {
  int index = (int)(uintptr_t)data;
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

  const bool animated = true;
  app_window_stack_remove(&schedule_window->window, animated);
}

static void prv_time_range_select_window_push(int index,
                                              SettingsQuietTimeScheduleData *data) {
  QuietTimeScheduleConfig config;
  quiet_time_get_schedule(index, &config);
  TimeRangeSelectionWindowData *schedule_window = &data->schedule_window;
  time_range_selection_window_init(schedule_window, GColorCobaltBlue,
                                   prv_complete_schedule, (void*)(uintptr_t)index);

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
  quiet_time_set_schedule_enabled(index, true);
  SettingsQuietTimeScheduleData *data = context;
  data->selected_schedule_index = index;
  prv_time_range_select_window_push(index, data);
}

static void prv_dnd_delete_schedule(ActionMenu *action_menu,
                                     const ActionMenuItem *item,
                                     void *context) {
  int index = (int)(uintptr_t)item->action_data;
  quiet_time_delete_schedule(index);
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
    .label = i18n_get("Change Schedule", &data->action_menu),
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
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Schedule sub-menu
///////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_schedule_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_schedule_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                     const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;
  int active_schedules = data->num_schedules;
  bool can_add = (active_schedules < MAX_QUIET_TIME_SCHEDULES);

  const char *title = NULL;
  bool title_needs_i18n = true;
  char *subtitle = NULL;
  const uint8_t buffer_length = 80;
  char title_buf[buffer_length];

  if (row == 0) {
    title = i18n_get("Calendar Aware", data);
    title_needs_i18n = false;
    subtitle = app_malloc_check(buffer_length);
    strncpy(subtitle, do_not_disturb_is_smart_dnd_enabled() ?
              i18n_ctx_get("QuietTime", "Enabled", data) :
              i18n_ctx_get("QuietTime", "Disabled", data), buffer_length);
  } else if (row <= (uint16_t)active_schedules) {
    int idx = row - 1;
    QuietTimeScheduleConfig *config = &data->schedules[idx];
    if (config->kind == QT_KIND_CUSTOM) {
      // "Custom: Mon,Wed,Fri" — show day list in title (i18n'd, no per-call alloc)
      i18n_get_with_buffer(quiet_time_get_string_for_kind(QT_KIND_CUSTOM),
                           title_buf, sizeof(title_buf));
      size_t used = strlen(title_buf);
      if (used + 2 < sizeof(title_buf)) {
        title_buf[used++] = ':';
        title_buf[used++] = ' ';
        title_buf[used] = '\0';
      }
      quiet_time_get_string_for_custom(config->scheduled_days,
                                       title_buf + used, sizeof(title_buf) - used);
      title = title_buf;
      title_needs_i18n = false;
    } else {
      title = quiet_time_get_string_for_kind(config->kind);
    }
    subtitle = app_malloc_check(buffer_length);
    if (config->enabled) {
      prv_get_qt_time(config, subtitle, buffer_length);
    } else {
      strncpy(subtitle, i18n_ctx_get("QuietTime", "Disabled", data), buffer_length);
    }
  } else if (can_add && row == (uint16_t)(active_schedules + 1)) {
    title = i18n_get("+ Add Schedule", data);
    title_needs_i18n = false;
    subtitle = NULL;
  } else {
    WTF;
  }

  const char *display_title = title_needs_i18n ? i18n_get(title, data) : title;
  menu_cell_basic_draw(ctx, cell_layer, display_title, subtitle, NULL);
  if (subtitle) {
    app_free(subtitle);
  }
}

static void prv_schedule_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;
  int active_schedules = data->num_schedules;
  bool can_add = (active_schedules < MAX_QUIET_TIME_SCHEDULES);

  if (row == 0) {
    do_not_disturb_toggle_smart_dnd();
  } else if (row <= (uint16_t)active_schedules) {
    int idx = row - 1;
    data->selected_schedule_index = idx;
    prv_scheduled_dnd_menu_push(idx, data);
  } else if (can_add && row == (uint16_t)(active_schedules + 1)) {
    DayPickerResult initial = {.kind = DayPickerKindEveryday};
    memset(initial.custom_days, 0, sizeof(initial.custom_days));
    DayPickerConfig config = {
      .initial = initial,
      .highlight_color = shell_prefs_get_theme_highlight_color(),
      .allow_once = false,
    };
    day_picker_push(config, prv_add_day_picker_callback, data);
  }
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static uint16_t prv_schedule_num_rows_cb(SettingsCallbacks *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *) context;
  int active_schedules = data->num_schedules;
  bool can_add = (active_schedules < MAX_QUIET_TIME_SCHEDULES);
  return 1 + active_schedules + (can_add ? 1 : 0);
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
