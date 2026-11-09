/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "quiet_time.h"

#include "menu.h"
#include "window.h"

#include <stdio.h>

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/fatal.h>
#include <pbl/services/alarms/alarm.h>
#include <pbl/services/clock.h>
#include <pbl/services/notifications/alerts_preferences.h>
#include <pbl/services/notifications/alerts_private.h>
#include <pbl/services/notifications/do_not_disturb.h>
#include <pbl/util/size.h>
#include <pbl/util/string.h>

#include <applib/app_malloc.h>
#include <applib/ui/action_menu_window_private.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/day_picker.h>
#include <applib/ui/menu_layer.h>
#include <applib/ui/time_range_selection_window.h>
#include <resource/resource_ids.auto.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

typedef struct {
  SettingsCallbacks callbacks;
} SettingsQuietTimeData;

typedef struct {
  SettingsCallbacks callbacks;
  DoNotDisturbScheduleSlot slots[DND_NUM_SCHEDULE_SLOTS];
  unsigned int row_slots[DND_NUM_SCHEDULE_SLOTS];
  unsigned int num_slots;
  unsigned int selected_slot;
  DoNotDisturbScheduleSlot new_slot;
  bool adding;
  char *action_menu_text;
  TimeRangeSelectionWindowData schedule_window;
  bool schedule_window_initialized;
  ActionMenuConfig action_menu;
  GBitmap add_icon;
  uint32_t add_icon_resource_id;
  bool show_limit_reached_text;
} SettingsQuietTimeScheduleData;

typedef struct {
  SettingsCallbacks callbacks;
} SettingsQuietTimeBacklightData;

enum QuietTimeItem {
  QuietTimeItemManual,
  QuietTimeItemSmartDnd,
  QuietTimeItemSchedule,
  QuietTimeItemInterruptions,
  QuietTimeItemNotifications,
  QuietTimeItemBacklight,
#ifdef CONFIG_SPEAKER
  QuietTimeItemMuteSpeaker,
#endif
  QuietTimeItem_Count,
};

static_assert((int)DoNotDisturbScheduleKindEveryday == (int)DayPickerKindEveryday);
static_assert((int)DoNotDisturbScheduleKindWeekdays == (int)DayPickerKindWeekdays);
static_assert((int)DoNotDisturbScheduleKindWeekends == (int)DayPickerKindWeekends);
static_assert((int)DoNotDisturbScheduleKindCustom == (int)DayPickerKindCustom);

enum QuietTimeBacklightItem {
  QuietTimeBacklightItemMotion,
#ifdef CONFIG_TOUCH
  QuietTimeBacklightItemTouch,
#endif
  QuietTimeBacklightItemNotifications,
  QuietTimeBacklightItem_Count,
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
  const char *title = nullptr;
  switch (alerts_get_dnd_mask()) {
    case AlertMaskAllOff:
      title = pbl_i18n_get("Quiet All Notifications", i18n_key);
      break;
    case AlertMaskPhoneCalls:
      title = pbl_i18n_get("Allow Phone Calls", i18n_key);
      break;
    default:
      title = "???";
      break;
  }
  return title;
}

static void prv_get_dnd_time(const DoNotDisturbSchedule *schedule, char *time_string,
                             const uint8_t len) {
  clock_format_time(time_string, len, schedule->from_hour, schedule->from_minute, true);
  strcat(time_string, " - ");
  uint8_t current_length = strnlen(time_string, len);
  char *buffer = time_string + current_length;
  clock_format_time(buffer, len - current_length, schedule->to_hour, schedule->to_minute, true);
}

static void prv_load_schedules(SettingsQuietTimeScheduleData *data) {
  data->num_slots = 0;
  for (unsigned int i = 0; i < DND_NUM_SCHEDULE_SLOTS; i++) {
    do_not_disturb_get_schedule_slot(i, &data->slots[i]);
    if (data->slots[i].used) {
      data->row_slots[data->num_slots++] = i;
    }
  }
}

static void prv_reload_schedules(SettingsQuietTimeScheduleData *data) {
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static DayPickerResult prv_get_day_picker_result(const DoNotDisturbScheduleSlot *slot) {
  DayPickerResult result = {
    .kind = (DayPickerKind)slot->kind,
  };
  const uint8_t days = do_not_disturb_schedule_slot_get_days(slot);
  for (int i = 0; i < PBL_DAY_PER_WEEK; i++) {
    result.custom_days[i] = days & (1 << i);
  }
  return result;
}

static void prv_set_slot_days(DoNotDisturbScheduleSlot *slot, const DayPickerResult *result) {
  slot->kind = result->kind;
  slot->days = 0;
  if (result->kind == DayPickerKindCustom) {
    for (int i = 0; i < PBL_DAY_PER_WEEK; i++) {
      if (result->custom_days[i]) {
        slot->days |= 1 << i;
      }
    }
  }
}

static void prv_day_picker_push(DayPickerResult initial, DayPickerCallback callback,
                                SettingsQuietTimeScheduleData *data) {
  const DayPickerConfig config = {
    .initial = initial,
    .highlight_color = shell_prefs_get_theme_highlight_color(),
  };
  day_picker_push(config, callback, data);
}

///////////////////////////////
// DND Action Menu Window
///////////////////////////////

enum {
  DNDMenuItemDisable = 0,
  DNDMenuItemChangeSchedule,
  DNDMenuItemChangeDays,
  DNDMenuItemDelete,
  DNDMenuItem_Count
};

//! Read the selected slot again, as the phone may have changed it since the list was loaded
static bool prv_get_selected_slot(SettingsQuietTimeScheduleData *data,
                                  DoNotDisturbScheduleSlot *slot) {
  do_not_disturb_get_schedule_slot(data->selected_slot, slot);
  return slot->used;
}

static void prv_toggle_scheduled_dnd(ActionMenu *action_menu, const ActionMenuItem *item,
                                     void *context) {
  SettingsQuietTimeScheduleData *data = context;
  DoNotDisturbScheduleSlot slot;
  if (prv_get_selected_slot(data, &slot)) {
    slot.enabled = !slot.enabled;
    do_not_disturb_set_schedule_slot(data->selected_slot, &slot);
  }
  prv_reload_schedules(data);
}

static void prv_complete_schedule(TimeRangeSelectionWindowData *schedule_window, void *context) {
  SettingsQuietTimeScheduleData *data = context;
  DoNotDisturbScheduleSlot slot = data->new_slot;
  if (!data->adding && !prv_get_selected_slot(data, &slot)) {
    prv_reload_schedules(data);
    window_stack_remove(&schedule_window->window, true /* animated */);
    return;
  }
  slot.enabled = true;
  slot.schedule = (DoNotDisturbSchedule){
    .from_hour = schedule_window->from.hour,
    .from_minute = schedule_window->from.minute,
    .to_hour = schedule_window->to.hour,
    .to_minute = schedule_window->to.minute,
  };

  DoNotDisturbSchedule *schedule = &slot.schedule;
  if (schedule->from_hour == schedule->to_hour && schedule->from_minute == schedule->to_minute) {
    if ((schedule->to_minute = (schedule->to_minute + 1) % 60) == 0) {
      schedule->to_hour = (schedule->to_hour + 1) % 24;
    }
  }

  if (data->adding) {
    do_not_disturb_add_schedule_slot(&slot);
  } else {
    do_not_disturb_set_schedule_slot(data->selected_slot, &slot);
  }
  prv_reload_schedules(data);

  const bool animated = true;
  window_stack_remove(&schedule_window->window, animated);
}

static void prv_time_range_select_window_push(const DoNotDisturbSchedule *schedule,
                                              SettingsQuietTimeScheduleData *data) {
  TimeRangeSelectionWindowData *schedule_window = &data->schedule_window;
  if (data->schedule_window_initialized) {
    time_range_selection_window_deinit(schedule_window);
  }
  time_range_selection_window_init(schedule_window, GColorCobaltBlue, prv_complete_schedule, data);
  data->schedule_window_initialized = true;

  schedule_window->from.hour = schedule->from_hour;
  schedule_window->from.minute = schedule->from_minute;
  schedule_window->to.hour = schedule->to_hour;
  schedule_window->to.minute = schedule->to_minute;
  app_window_stack_push(&schedule_window->window, true);
}

static void prv_dnd_set_schedule(ActionMenu *action_menu, const ActionMenuItem *item,
                                 void *context) {
  SettingsQuietTimeScheduleData *data = context;
  data->adding = false;
  prv_time_range_select_window_push(&data->slots[data->selected_slot].schedule, data);
}

static void prv_change_days_callback(DayPickerResult result, void *context) {
  SettingsQuietTimeScheduleData *data = context;
  DoNotDisturbScheduleSlot slot;
  if (prv_get_selected_slot(data, &slot)) {
    prv_set_slot_days(&slot, &result);
    do_not_disturb_set_schedule_slot(data->selected_slot, &slot);
  }
  prv_reload_schedules(data);
}

static void prv_dnd_change_days(ActionMenu *action_menu, const ActionMenuItem *item,
                                void *context) {
  SettingsQuietTimeScheduleData *data = context;
  prv_day_picker_push(prv_get_day_picker_result(&data->slots[data->selected_slot]),
                      prv_change_days_callback, data);
}

static void prv_dnd_delete_schedule(ActionMenu *action_menu, const ActionMenuItem *item,
                                    void *context) {
  SettingsQuietTimeScheduleData *data = context;
  do_not_disturb_delete_schedule_slot(data->selected_slot);
  prv_reload_schedules(data);
}

static void prv_scheduled_dnd_menu_cleanup(ActionMenu *action_menu, const ActionMenuItem *item,
                                           void *context) {
  ActionMenuLevel *root_level = action_menu_get_root_level(action_menu);
  SettingsQuietTimeScheduleData *data = context;
  app_free(data->action_menu_text);
  pbl_i18n_free_all(&data->action_menu);
  app_free(root_level);
}

static void prv_scheduled_dnd_menu_push(SettingsQuietTimeScheduleData *data) {
  const DoNotDisturbScheduleSlot *slot = &data->slots[data->selected_slot];
  data->action_menu = (ActionMenuConfig){
    .context = data,
    .colors.background = shell_prefs_get_theme_highlight_color(),
    .did_close = prv_scheduled_dnd_menu_cleanup,
  };

  ActionMenuLevel *level =
      app_malloc_check(sizeof(ActionMenuLevel) + DNDMenuItem_Count * sizeof(ActionMenuItem));
  *level = (ActionMenuLevel){
    .num_items = DNDMenuItem_Count,
    .display_mode = ActionMenuLevelDisplayModeWide,
  };
  const int text_max_size = 30;
  char time_text[24];
  // Label, " (", time and ")"
  const size_t buffer_size = text_max_size + 2 + sizeof(time_text) + 1;
  data->action_menu_text = app_malloc_check(buffer_size);
  if (slot->enabled) {
    snprintf(data->action_menu_text, buffer_size, "%.*s", text_max_size,
             pbl_i18n_get("Disable", &data->action_menu));
  } else {
    prv_get_dnd_time(&slot->schedule, time_text, sizeof(time_text));
    snprintf(data->action_menu_text, buffer_size, "%.*s (%s)", text_max_size,
             pbl_i18n_get("Enable", &data->action_menu), time_text);
  }

  level->items[DNDMenuItemDisable] = (ActionMenuItem){
    .label = data->action_menu_text,
    .perform_action = prv_toggle_scheduled_dnd,
  };

  level->items[DNDMenuItemChangeSchedule] = (ActionMenuItem){
    .label = pbl_i18n_get("Change Schedule", &data->action_menu),
    .perform_action = prv_dnd_set_schedule,
  };

  level->items[DNDMenuItemChangeDays] = (ActionMenuItem){
    .label = pbl_i18n_get("Change Days", &data->action_menu),
    .perform_action = prv_dnd_change_days,
  };

  level->items[DNDMenuItemDelete] = (ActionMenuItem){
    .label = pbl_i18n_get("Delete", &data->action_menu),
    .perform_action = prv_dnd_delete_schedule,
  };

  data->action_menu.root_level = level;
  app_action_menu_open(&data->action_menu);
}

///////////////////////////////
// Schedule sub-menu
///////////////////////////////

static void prv_add_day_picker_callback(DayPickerResult result, void *context) {
  SettingsQuietTimeScheduleData *data = context;
  data->new_slot = (DoNotDisturbScheduleSlot){
    .schedule = {
      .from_hour = 22,
      .to_hour = 7,
    },
  };
  prv_set_slot_days(&data->new_slot, &result);
  data->adding = true;
  prv_time_range_select_window_push(&data->new_slot.schedule, data);
}

static void prv_schedule_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *)context;
  if (data->schedule_window_initialized) {
    time_range_selection_window_deinit(&data->schedule_window);
  }
  gbitmap_deinit(&data->add_icon);
  pbl_i18n_free_all(data);
  app_free(data);
}

static void prv_draw_add_schedule_row(SettingsQuietTimeScheduleData *data, GContext *ctx,
                                      const Layer *cell_layer) {
  GRect box;
  uint32_t new_bitmap_resource = RESOURCE_ID_PLUS_ICON_BLACK;

  if (data->num_slots == DND_NUM_SCHEDULE_SLOTS) {
    if (menu_cell_layer_is_highlighted(cell_layer)) {
      if (data->show_limit_reached_text) {
        const GFont font = system_theme_get_font(TextStyleFont_MenuCellSubtitle);

        box = GRect(0, 0, cell_layer->bounds.size.w, fonts_get_font_height(font));

        const char *text = pbl_i18n_get("Limit reached.", data);
        box.size = graphics_text_layout_get_max_used_size(
            ctx, text, font, box, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, nullptr);
        grect_align(&box, &cell_layer->bounds, GAlignCenter, true /* clip */);
        box.origin.y -= fonts_get_font_cap_offset(font);

        graphics_draw_text(ctx, text, font, box, GTextOverflowModeFill, GTextAlignmentCenter,
                           nullptr);
        return;
      }
      new_bitmap_resource = RESOURCE_ID_PLUS_ICON_DOTTED;
    } else {
      graphics_context_set_tint_color(ctx, GColorLightGray);
    }
  }

  if (new_bitmap_resource != data->add_icon_resource_id) {
    data->add_icon_resource_id = new_bitmap_resource;
    gbitmap_deinit(&data->add_icon);
    gbitmap_init_with_resource(&data->add_icon, data->add_icon_resource_id);
  }

  box.origin = GPoint((cell_layer->bounds.size.w - data->add_icon.bounds.size.w) / 2,
                      (cell_layer->bounds.size.h - data->add_icon.bounds.size.h) / 2);
  box.size = data->add_icon.bounds.size;
  graphics_context_set_compositing_mode(ctx, GCompOpTint);
  graphics_draw_bitmap_in_rect(ctx, &data->add_icon, &box);
}

static void prv_schedule_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                     const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *)context;
  if (row == 0) {
    prv_draw_add_schedule_row(data, ctx, cell_layer);
    return;
  }

  const DoNotDisturbScheduleSlot *slot = &data->slots[data->row_slots[row - 1]];
  char time_text[32];
  prv_get_dnd_time(&slot->schedule, time_text, sizeof(time_text));
  char day_text[32] = {};
  MenuCellLayerConfig config = {
    .title = time_text,
    .value = slot->enabled ? pbl_i18n_get("ON", data) : pbl_i18n_get("OFF", data),
    .horizontal_inset = PBL_IF_ROUND_ELSE(-6, 0),
    .overflow_mode = GTextOverflowModeTrailingEllipsis,
  };
  if (slot->kind == DoNotDisturbScheduleKindCustom) {
    DayPickerResult days = prv_get_day_picker_result(slot);
    alarm_get_string_for_custom(days.custom_days, day_text);
    config.subtitle = day_text;
  } else {
    config.subtitle = pbl_i18n_get(day_picker_kind_get_string((DayPickerKind)slot->kind), data);
  }
  menu_cell_layer_draw(ctx, cell_layer, &config);
}

static void prv_schedule_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *)context;

  if (row == 0) {
    if (data->num_slots == DND_NUM_SCHEDULE_SLOTS) {
      data->show_limit_reached_text = true;
    } else {
      prv_day_picker_push((DayPickerResult){.kind = DayPickerKindEveryday},
                          prv_add_day_picker_callback, data);
    }
  } else {
    data->selected_slot = data->row_slots[row - 1];
    prv_scheduled_dnd_menu_push(data);
  }
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static void prv_schedule_selection_changed_cb(SettingsCallbacks *context, uint16_t new_row,
                                              uint16_t old_row) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *)context;
  if (old_row == 0) {
    data->show_limit_reached_text = false;
  }
}

static uint16_t prv_schedule_num_rows_cb(SettingsCallbacks *context) {
  SettingsQuietTimeScheduleData *data = (SettingsQuietTimeScheduleData *)context;
  // Every reload passes here, including the one for a pref synced by the phone
  prv_load_schedules(data);
  return 1 + data->num_slots;
}

static void prv_schedule_submenu_push(void) {
  SettingsQuietTimeScheduleData *data = app_zalloc_check(sizeof(*data));

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_schedule_deinit_cb,
    .draw_row = prv_schedule_draw_row_cb,
    .select_click = prv_schedule_select_click_cb,
    .selection_changed = prv_schedule_selection_changed_cb,
    .num_rows = prv_schedule_num_rows_cb,
  };

  Window *window = settings_window_create_with_title(SettingsMenuItemQuietTime,
                                                     pbl_i18n_noop("Schedule"), &data->callbacks);
  prv_load_schedules(data);
  app_window_stack_push(window, true /* animated */);
}

static const char *prv_get_dnd_notifications_subtitle(void *i18n_key) {
  if (alerts_preferences_dnd_get_show_notifications() == DndNotificationModeHide) {
    return pbl_i18n_get("Off", i18n_key);
  }
  if (alerts_preferences_dnd_get_auto_dismiss()) {
    return pbl_i18n_get("On - Auto Dismiss", i18n_key);
  }
  return pbl_i18n_get("On - Persistent", i18n_key);
}

static void prv_cycle_dnd_notifications(void) {
  if (alerts_preferences_dnd_get_show_notifications() == DndNotificationModeHide) {
    alerts_preferences_dnd_set_show_notifications(DndNotificationModeShow);
    alerts_preferences_dnd_set_auto_dismiss(false);
  } else if (!alerts_preferences_dnd_get_auto_dismiss()) {
    alerts_preferences_dnd_set_auto_dismiss(true);
  } else {
    alerts_preferences_dnd_set_show_notifications(DndNotificationModeHide);
    alerts_preferences_dnd_set_auto_dismiss(false);
  }
}

///////////////////////////////
// Backlight sub-menu
///////////////////////////////

static void prv_backlight_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeBacklightData *data = (SettingsQuietTimeBacklightData *)context;
  pbl_i18n_free_all(data);
  app_free(data);
}

static void prv_backlight_draw_row_cb(SettingsCallbacks *context, GContext *ctx,
                                      const Layer *cell_layer, uint16_t row, bool selected) {
  SettingsQuietTimeBacklightData *data = (SettingsQuietTimeBacklightData *)context;
  const char *title = nullptr;
  const char *subtitle = nullptr;
  switch (row) {
    case QuietTimeBacklightItemMotion:
      title = pbl_i18n_noop("Motion");
      subtitle = alerts_preferences_dnd_get_motion_backlight() ? pbl_i18n_noop("On")
                                                               : pbl_i18n_noop("Off");
      break;
#ifdef CONFIG_TOUCH
    case QuietTimeBacklightItemTouch:
      title = pbl_i18n_noop("Touch");
      subtitle =
          alerts_preferences_dnd_get_touch_backlight() ? pbl_i18n_noop("On") : pbl_i18n_noop("Off");
      break;
#endif
    case QuietTimeBacklightItemNotifications:
      title = pbl_i18n_noop("Notifications");
      subtitle = alerts_preferences_dnd_get_notification_backlight() ? pbl_i18n_noop("On")
                                                                     : pbl_i18n_noop("Off");
      break;
    default:
      pbl_panic();
  }
  menu_cell_basic_draw(ctx, cell_layer, pbl_i18n_get(title, data), pbl_i18n_get(subtitle, data),
                       nullptr);
}

static void prv_backlight_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  switch (row) {
    case QuietTimeBacklightItemMotion:
      alerts_preferences_dnd_set_motion_backlight(!alerts_preferences_dnd_get_motion_backlight());
      break;
#ifdef CONFIG_TOUCH
    case QuietTimeBacklightItemTouch:
      alerts_preferences_dnd_set_touch_backlight(!alerts_preferences_dnd_get_touch_backlight());
      break;
#endif
    case QuietTimeBacklightItemNotifications:
      alerts_preferences_dnd_set_notification_backlight(
          !alerts_preferences_dnd_get_notification_backlight());
      break;
    default:
      pbl_panic();
  }
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static uint16_t prv_backlight_num_rows_cb(SettingsCallbacks *context) {
  return QuietTimeBacklightItem_Count;
}

static void prv_backlight_submenu_push(void) {
  SettingsQuietTimeBacklightData *data = app_zalloc_check(sizeof(*data));

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_backlight_deinit_cb,
    .draw_row = prv_backlight_draw_row_cb,
    .select_click = prv_backlight_select_click_cb,
    .num_rows = prv_backlight_num_rows_cb,
  };

  Window *window = settings_window_create_with_title(SettingsMenuItemQuietTime,
                                                     pbl_i18n_noop("Backlight"), &data->callbacks);
  app_window_stack_push(window, true /* animated */);
}

///////////////////////////////
// Top-level Quiet Time menu
///////////////////////////////

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsQuietTimeData *data = (SettingsQuietTimeData *)context;
  pbl_i18n_free_all(data);
  app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx, const Layer *cell_layer,
                            uint16_t row, bool selected) {
  SettingsQuietTimeData *data = (SettingsQuietTimeData *)context;
  const char *title = nullptr;
  const char *subtitle = nullptr;

  switch (row) {
    case QuietTimeItemManual:
      title = pbl_i18n_get("Manual", data);
      subtitle = do_not_disturb_is_manually_enabled() ? pbl_i18n_get("On", data)
                                                      : pbl_i18n_get("Off", data);
      break;
    case QuietTimeItemSmartDnd:
      title = pbl_i18n_get("Calendar Aware", data);
      subtitle = do_not_disturb_is_smart_dnd_enabled() ? pbl_i18n_get("On", data)
                                                       : pbl_i18n_get("Off", data);
      break;
    case QuietTimeItemSchedule:
      title = pbl_i18n_get("Schedule", data);
      break;
    case QuietTimeItemInterruptions:
      title = pbl_i18n_get("Interruptions", data);
      subtitle = prv_get_dnd_mask_subtitle(data);
      break;
    case QuietTimeItemNotifications:
      title = pbl_i18n_get("Notifications", data);
      subtitle = prv_get_dnd_notifications_subtitle(data);
      break;
    case QuietTimeItemBacklight:
      title = pbl_i18n_get("Backlight", data);
      break;
#ifdef CONFIG_SPEAKER
    case QuietTimeItemMuteSpeaker:
      title = pbl_i18n_get("Mute Speaker", data);
      subtitle = alerts_preferences_dnd_get_mute_speaker() ? pbl_i18n_get("On", data)
                                                           : pbl_i18n_get("Off", data);
      break;
#endif
    default:
      pbl_panic();
  }
  menu_cell_basic_draw(ctx, cell_layer, title, subtitle, nullptr);
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
      prv_cycle_dnd_notifications();
      break;
    case QuietTimeItemBacklight:
      prv_backlight_submenu_push();
      break;
#ifdef CONFIG_SPEAKER
    case QuietTimeItemMuteSpeaker:
      alerts_preferences_dnd_set_mute_speaker(!alerts_preferences_dnd_get_mute_speaker());
      break;
#endif
    default:
      pbl_panic();
  }
  settings_menu_reload_data(SettingsMenuItemQuietTime);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  return QuietTimeItem_Count;
}

static Window *prv_init(void) {
  SettingsQuietTimeData *data = app_zalloc_check(sizeof(*data));

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
  };

  return settings_window_create(SettingsMenuItemQuietTime, &data->callbacks);
}

const SettingsModuleMetadata *settings_quiet_time_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = pbl_i18n_noop("Quiet Time"),
    .init = prv_init,
  };

  return &s_module_info;
}
