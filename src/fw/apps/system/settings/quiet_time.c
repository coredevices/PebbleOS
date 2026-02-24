/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "quiet_time.h"
#include "menu.h"
#include "window.h"

#include "applib/ui/action_menu_window_private.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/menu_layer.h"
#include "applib/ui/time_range_selection_window.h"
#include "kernel/pbl_malloc.h"
#include "popups/health_tracking_ui.h"
#include "services/common/clock.h"
#include "services/common/i18n/i18n.h"
#include "services/normal/activity/activity.h"
#include "services/normal/notifications/alerts_private.h"
#include "services/normal/notifications/do_not_disturb.h"
#include "services/normal/notifications/alerts_preferences.h"
#include "services/normal/notifications/alerts_preferences_private.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"
#include "util/string.h"
#include "shell/prefs.h"

#include <stdio.h>

typedef struct {
  SettingsCallbacks callbacks;

  char *action_menu_text;

  TimeRangeSelectionWindowData schedule_window;
  ActionMenuConfig action_menu;
} SettingsQuietTimeData;

enum QuietTimeItem {
  QuietTimeItemManual,
  QuietTimeItemCalendarAware,
  QuietTimeItemWeekdayScheduled,
  QuietTimeItemWeekendScheduled,
  QuietTimeItemInterruptions,
  QuietTimeItemNotifications,
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

///////////////////////////////
// DND Action Menu Window
///////////////////////////////

enum {
  DNDMenuItemDisable = 0,
  DNDMenuItemChangeSchedule,
  DNDMenuItem_Count
};

static void prv_toggle_scheduled_dnd(ActionMenu *action_menu,
                                     const ActionMenuItem *item,
                                     void *context) {
  do_not_disturb_toggle_scheduled((DoNotDisturbScheduleType) item->action_data);
}

static void prv_complete_schedule(TimeRangeSelectionWindowData *schedule_window, void *data) {
  DoNotDisturbScheduleType type = (DoNotDisturbScheduleType) data;
  DoNotDisturbSchedule schedule = {
    .from_hour = schedule_window->from.hour,
    .from_minute = schedule_window->from.minute,
    .to_hour = schedule_window->to.hour,
    .to_minute = schedule_window->to.minute,
  };

  if (schedule.from_hour == schedule.to_hour && schedule.from_minute == schedule.to_minute) {
    if ((schedule.to_minute = (schedule.to_minute + 1) % 60) == 0) {
      schedule.to_hour = (schedule.to_hour + 1) % 24;
    }
  }

  do_not_disturb_set_schedule(type, &schedule);

  const bool animated = true;
  window_stack_remove(&schedule_window->window, animated);
}

static void prv_time_range_select_window_push(DoNotDisturbScheduleType type,
                                              SettingsQuietTimeData *data) {
  DoNotDisturbSchedule schedule;
  do_not_disturb_get_schedule(type, &schedule);
  TimeRangeSelectionWindowData *schedule_window = &data->schedule_window;
  time_range_selection_window_init(schedule_window, GColorCobaltBlue,
                                   prv_complete_schedule, (void*)(uintptr_t) type);

  schedule_window->from.hour = schedule.from_hour;
  schedule_window->from.minute = schedule.from_minute;
  schedule_window->to.hour = schedule.to_hour;
  schedule_window->to.minute = schedule.to_minute;
  app_window_stack_push(&schedule_window->window, true);
}

static void prv_dnd_set_schedule(ActionMenu *action_menu,
                                    const ActionMenuItem *item,
                                    void *context) {
  DoNotDisturbScheduleType type = (DoNotDisturbScheduleType) item->action_data;
  do_not_disturb_set_schedule_enabled(type, true);
  prv_time_range_select_window_push(type, context);
}

static void prv_scheduled_dnd_menu_cleanup(ActionMenu *action_menu,
                                 const ActionMenuItem *item,
                                 void *context) {
  ActionMenuLevel *root_level = action_menu_get_root_level(action_menu);
  SettingsQuietTimeData *data = context;
  time_range_selection_window_deinit(&data->schedule_window);
  app_free(data->action_menu_text);
  i18n_free_all(&data->action_menu);
  task_free(root_level);
}

static void prv_get_dnd_time(DoNotDisturbScheduleType type, char *time_string, const uint8_t len) {
  DoNotDisturbSchedule schedule;
  do_not_disturb_get_schedule(type, &schedule);

  clock_format_time(time_string, len, schedule.from_hour, schedule.from_minute, true);
  strcat(time_string, " - ");
  uint8_t current_length = strnlen(time_string, len);
  char *buffer = time_string + current_length;
  clock_format_time(buffer, len - current_length, schedule.to_hour, schedule.to_minute, true);
}

static void prv_scheduled_dnd_menu_push(DoNotDisturbScheduleType type,
                                        SettingsQuietTimeData *data) {
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
  const uint8_t text_max_size = 30;
  const uint8_t buffer_size = text_max_size + 22;
  data->action_menu_text = app_malloc_check(buffer_size);
  if (do_not_disturb_is_schedule_enabled(type)) {
    strncpy(data->action_menu_text, i18n_get("Disable", &data->action_menu), buffer_size);
  } else {
    strncpy(data->action_menu_text, i18n_get("Enable", &data->action_menu), text_max_size);
    strcat(data->action_menu_text, " (");
    uint8_t current_length = strnlen(data->action_menu_text, buffer_size);
    char *buffer = data->action_menu_text + current_length;
    prv_get_dnd_time(type, buffer, buffer_size - current_length);
    strcat(data->action_menu_text, ")");
  }

  level->items[DNDMenuItemDisable] = (ActionMenuItem) {
    .label = data->action_menu_text,
    .perform_action = prv_toggle_scheduled_dnd,
  };

  level->items[DNDMenuItemChangeSchedule] = (ActionMenuItem) {
    .label = i18n_get("Change Schedule", &data->action_menu),
    .perform_action = prv_dnd_set_schedule,
  };

  if (type == WeekdaySchedule) {
    level->items[DNDMenuItemDisable].action_data = (void*)(uintptr_t) WeekdaySchedule;
    level->items[DNDMenuItemChangeSchedule].action_data = (void*)(uintptr_t) WeekdaySchedule;
  } else {
    level->items[DNDMenuItemDisable].action_data = (void*)(uintptr_t) WeekendSchedule;
    level->items[DNDMenuItemChangeSchedule].action_data = (void*)(uintptr_t) WeekendSchedule;
  }

  data->action_menu.root_level = level;
  app_action_menu_open(&data->action_menu);
}

///////////////////////////////
// Menu Layer Callbacks
///////////////////////////////

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeData *data = (SettingsQuietTimeData *) context;
  i18n_free_all(data);
  app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                            const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsQuietTimeData *data = (SettingsQuietTimeData *) context;
  const char *title = NULL;
  char *subtitle = NULL;
  const uint8_t buffer_length = 80;
  subtitle = app_malloc_check(buffer_length);

  switch (row) {
    case QuietTimeItemManual:
      title = i18n_get("Manual", data);
      strncpy(subtitle, do_not_disturb_is_manually_enabled() ?
                  i18n_get("On", data) : i18n_get("Off", data), buffer_length);
      break;
    case QuietTimeItemCalendarAware:
      title = i18n_get("Calendar Aware", data);
      strncpy(subtitle, do_not_disturb_is_smart_dnd_enabled() ?
                i18n_ctx_get("QuietTime", "Enabled", data) :
                i18n_ctx_get("QuietTime", "Disabled", data), buffer_length);
      break;
    case QuietTimeItemWeekdayScheduled:
      title = i18n_get("Weekdays", data);
      if (do_not_disturb_is_schedule_enabled(WeekdaySchedule)) {
        prv_get_dnd_time(WeekdaySchedule, subtitle, buffer_length);
      } else {
        strncpy(subtitle, i18n_ctx_get("QuietTime", "Disabled", data), buffer_length);
      }
      break;
    case QuietTimeItemWeekendScheduled:
      title = i18n_get("Weekends", data);
      if (do_not_disturb_is_schedule_enabled(WeekendSchedule)) {
        prv_get_dnd_time(WeekendSchedule, subtitle, buffer_length);
      } else {
        strncpy(subtitle, i18n_ctx_get("QuietTime", "Disabled", data), buffer_length);
      }
      break;
    case QuietTimeItemInterruptions:
      title = i18n_get("Interruptions", data);
      strncpy(subtitle, prv_get_dnd_mask_subtitle(data), buffer_length);
      break;
    case QuietTimeItemNotifications:
      title = i18n_get("Notifications", data);
      strncpy(subtitle, prv_get_dnd_notifications_enable(data), buffer_length);
      break;
    default:
        WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
  app_free(subtitle);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsQuietTimeData *data = (SettingsQuietTimeData *) context;

  switch (row) {
    case QuietTimeItemManual:
      do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSourceSettingsMenu);
      break;
    case QuietTimeItemCalendarAware:
      do_not_disturb_toggle_smart_dnd();
      break;
    case QuietTimeItemWeekdayScheduled:
      prv_scheduled_dnd_menu_push(WeekdaySchedule, data);
      break;
    case QuietTimeItemWeekendScheduled:
      prv_scheduled_dnd_menu_push(WeekendSchedule, data);
      break;
    case QuietTimeItemInterruptions:
      prv_cycle_dnd_mask();
      break;
    case QuietTimeItemNotifications:
      prv_cycle_dnd_notification_mode();
      break;
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
