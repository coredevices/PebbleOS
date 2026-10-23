/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dark_mode_menu.h"

#include "option_menu.h"

#include <string.h>

#include <pbl/services/clock.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/size.h>

#include <applib/ui/ui.h>
#include <shell/prefs.h>

static const char * const s_dark_mode_labels[] = {
  [DarkModeOff] = i18n_noop("Off"),
  [DarkModeOn] = i18n_noop("On"),
  [DarkModeAmbient] = i18n_noop("Ambient"),
  [DarkModeScheduled] = i18n_noop("Schedule"),
};

static void prv_refresh_parent(DarkModeMenu *menu) {
  settings_menu_reload_data(menu->menu_item);
  settings_menu_mark_dirty(menu->menu_item);
}

static void prv_menu_select(OptionMenu *option_menu, int selection, void *context) {
  DarkModeMenu *menu = settings_option_menu_get_context(context);
  const DarkMode mode = (DarkMode)selection;
  shell_prefs_set_dark_mode(mode);
  if (mode == DarkModeScheduled) {
    app_window_stack_remove(&option_menu->window, false /* animated */);
    dark_mode_menu_schedule_push(menu);
  } else {
    app_window_stack_remove(&option_menu->window, true /* animated */);
  }
  prv_refresh_parent(menu);
}

static void prv_complete_schedule(TimeRangeSelectionWindowData *schedule_window, void *context) {
  DarkModeMenu *menu = context;
  DarkModeSchedule schedule = {
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
  shell_prefs_set_dark_mode_schedule(&schedule);
  prv_refresh_parent(menu);
  const bool animated = true;
  app_window_stack_remove(&schedule_window->window, animated);
}

void dark_mode_menu_init(DarkModeMenu *menu, SettingsMenuItem menu_item,
                         GColor (*get_highlight_color)(void)) {
  *menu = (DarkModeMenu){
    .menu_item = menu_item,
    .get_highlight_color = get_highlight_color,
  };
}

void dark_mode_menu_deinit(DarkModeMenu *menu) {
  time_range_selection_window_deinit(&menu->schedule_window);
}

void dark_mode_menu_push(DarkModeMenu *menu) {
  const int index = (int)shell_prefs_get_dark_mode();
  const OptionMenuCallbacks callbacks = {
    .select = prv_menu_select,
  };
  const char *title = PBL_IF_RECT_ELSE(i18n_noop("DARK MODE"), i18n_noop("Dark Mode"));
  settings_option_menu_push(
      title, OptionMenuContentType_SingleLine, index, &callbacks,
      ARRAY_LENGTH(s_dark_mode_labels), true /* icons_enabled */,
      (const char **)s_dark_mode_labels, menu);
}

void dark_mode_menu_schedule_push(DarkModeMenu *menu) {
  DarkModeSchedule schedule;
  shell_prefs_get_dark_mode_schedule(&schedule);
  TimeRangeSelectionWindowData *schedule_window = &menu->schedule_window;
  time_range_selection_window_init(schedule_window, menu->get_highlight_color(),
                                   prv_complete_schedule, menu);
  schedule_window->from.hour = schedule.from_hour;
  schedule_window->from.minute = schedule.from_minute;
  schedule_window->to.hour = schedule.to_hour;
  schedule_window->to.minute = schedule.to_minute;
  app_window_stack_push(&schedule_window->window, true);
}

const char *dark_mode_menu_get_mode_label(void) {
  return s_dark_mode_labels[shell_prefs_get_dark_mode()];
}

void dark_mode_menu_format_schedule(char *buf, size_t buf_size) {
  DarkModeSchedule schedule;
  shell_prefs_get_dark_mode_schedule(&schedule);
  clock_format_time(buf, buf_size, schedule.from_hour, schedule.from_minute, true);
  strcat(buf, " - ");
  const size_t len = strlen(buf);
  clock_format_time(buf + len, buf_size - len, schedule.to_hour, schedule.to_minute, true);
}
