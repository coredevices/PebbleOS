/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "alarm_editor.h"

#include "applib/pbl_std/timelocal.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/number_window.h"
#include "applib/ui/simple_menu_layer.h"
#include "applib/ui/time_selection_window.h"
#include "applib/ui/ui.h"
#include "apps/system_apps/settings/settings_option_menu.h"
#include "kernel/pbl_malloc.h"
#include "popups/health_tracking_ui.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "services/normal/activity/activity.h"
#include "services/normal/alarms/alarm.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"

#include <string.h>

#define ALARM_DAY_LIST_CELL_HEIGHT PBL_IF_RECT_ELSE(menu_cell_small_cell_height(), \
                                                    menu_cell_basic_cell_height())

typedef struct {
  OptionMenu *alarm_type_menu;
  AlarmType alarm_type;

  TimeSelectionWindowData time_picker_window;
  bool time_picker_was_completed;

  Window day_picker_window;
  MenuLayer day_picker_menu_layer;
  bool day_picker_was_completed;

  Window custom_day_picker_window;
  MenuLayer custom_day_picker_menu_layer;
  bool custom_day_picker_was_completed;
  bool scheduled_days[DAYS_PER_WEEK];
  GBitmap deselected_icon;
  GBitmap selected_icon;
  GBitmap checkmark_icon;
  uint32_t current_checkmark_icon_resource_id;
  bool show_check_something_first_text;

  AlarmEditorCompleteCallback complete_callback;
  void *callback_context;

  AlarmId alarm_id;
  int alarm_hour;
  int alarm_minute;
  AlarmKind alarm_kind;
  bool creating_alarm;
} AlarmEditorData;

typedef enum DayPickerMenuItems {
  DayPickerMenuItemsJustOnce = 0,
  DayPickerMenuItemsWeekdays,
  DayPickerMenuItemsWeekends,
  DayPickerMenuItemsEveryday,
  DayPickerMenuItemsCustom,
  DayPickerMenuItemsNumItems,
} DayPickerMenuItems;

// Forward Declarations
static void prv_setup_custom_day_picker_window(AlarmEditorData *data);
static bool prv_is_custom_day_scheduled(AlarmEditorData *data);

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Helper functions

static void prv_remove_windows(AlarmEditorData *data) {
  if (app_window_stack_contains_window(&data->time_picker_window.window)) {
    app_window_stack_remove(&data->time_picker_window.window, false);
  }
  if (app_window_stack_contains_window(&data->day_picker_window)) {
    app_window_stack_remove(&data->day_picker_window, false);
  }
  if (data->alarm_type_menu && app_window_stack_contains_window(&data->alarm_type_menu->window)) {
    app_window_stack_remove(&data->alarm_type_menu->window, false);
  }
}

static void prv_call_complete_cancelled_if_no_alarm(AlarmEditorData *data) {
  if (data->alarm_id == ALARM_INVALID_ID && data->complete_callback) {
    data->complete_callback(CANCELLED, data->alarm_id, data->callback_context);
  }
}

static DayPickerMenuItems prv_alarm_kind_to_index(AlarmKind alarm_kind) {
  switch (alarm_kind) {
    case ALARM_KIND_EVERYDAY:
      return DayPickerMenuItemsEveryday;
    case ALARM_KIND_WEEKENDS:
      return DayPickerMenuItemsWeekends;
    case ALARM_KIND_WEEKDAYS:
      return DayPickerMenuItemsWeekdays;
    case ALARM_KIND_JUST_ONCE:
      return DayPickerMenuItemsJustOnce;
    case ALARM_KIND_CUSTOM:
      return DayPickerMenuItemsCustom;
  }
  return DayPickerMenuItemsJustOnce;
}

static AlarmKind prv_index_to_alarm_kind(DayPickerMenuItems index) {
  switch (index) {
    case DayPickerMenuItemsWeekdays:
      return ALARM_KIND_WEEKDAYS;
    case DayPickerMenuItemsWeekends:
      return ALARM_KIND_WEEKENDS;
    case DayPickerMenuItemsEveryday:
      return ALARM_KIND_EVERYDAY;
    case DayPickerMenuItemsJustOnce:
      return ALARM_KIND_JUST_ONCE;
    case DayPickerMenuItemsCustom:
      return ALARM_KIND_CUSTOM;
    case DayPickerMenuItemsNumItems:
      break;
  }
  return ALARM_KIND_EVERYDAY;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Day Picker

static void prv_day_picker_window_unload(Window *window) {
  AlarmEditorData *data = (AlarmEditorData*) window_get_user_data(window);

  if (!data->day_picker_was_completed && data->time_picker_was_completed) {
    // If we cancel the day picker go back to the time picker
    data->time_picker_was_completed = false;
    return;
  }

  if (data->creating_alarm) {
    time_selection_window_deinit(&data->time_picker_window);
  }

  // Editing recurrence
  menu_layer_deinit(&data->day_picker_menu_layer);
  prv_remove_windows(data);

  i18n_free_all(&data->day_picker_window);
  task_free(data);
  data = NULL;
}

static void prv_handle_selection(int index, void *callback_context) {
  AlarmEditorData *data = (AlarmEditorData *)callback_context;
  data->day_picker_was_completed = true;

  data->alarm_kind = prv_index_to_alarm_kind(index);

  if (data->creating_alarm) {
    const AlarmInfo info = {
      .hour = data->alarm_hour,
      .minute = data->alarm_minute,
      .kind = data->alarm_kind,
      .is_smart = (data->alarm_type == AlarmType_Smart),
    };
    data->alarm_id = alarm_create(&info);
    data->complete_callback(CREATED, data->alarm_id, data->callback_context);
    app_window_stack_remove(&data->day_picker_window, true);
  } else {
    alarm_set_kind(data->alarm_id, data->alarm_kind);
    data->complete_callback(EDITED, data->alarm_id, data->callback_context);
    app_window_stack_pop(true);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Custom Day Picker

static void prv_custom_day_picker_window_unload(Window *window) {
  AlarmEditorData *data = (AlarmEditorData*) window_get_user_data(window);

  if (!data->custom_day_picker_was_completed) {
    // If we cancel the custom day picker go back to the day picker
    data->day_picker_was_completed = false;
    i18n_free_all(&data->custom_day_picker_window);
    return;
  }

  menu_layer_deinit(&data->custom_day_picker_menu_layer);
  prv_remove_windows(data);

  i18n_free_all(&data->custom_day_picker_window);
}

static void prv_handle_custom_day_selection(int index, void *callback_context) {
  AlarmEditorData *data = (AlarmEditorData *)callback_context;
  prv_setup_custom_day_picker_window(data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Menu Layer Callbacks

static uint16_t prv_day_picker_get_num_sections(struct MenuLayer *menu_layer,
                                                void *callback_context) {
  return 1;
}

static uint16_t prv_day_picker_get_num_rows(struct MenuLayer *menu_layer,
                                            uint16_t section_index,
                                            void *callback_context) {
  return DayPickerMenuItemsNumItems;
}

static int16_t prv_day_picker_get_cell_height(struct MenuLayer *menu_layer,
                                              MenuIndex *cell_index,
                                              void *callback_context) {
  return ALARM_DAY_LIST_CELL_HEIGHT;
}

static void prv_day_picker_draw_row(GContext *ctx, const Layer *cell_layer,
                                    MenuIndex *cell_index, void *callback_context) {
  AlarmEditorData *data = (AlarmEditorData *)callback_context;
  AlarmKind kind = prv_index_to_alarm_kind(cell_index->row);
  const bool all_caps = false;
  const char *cell_text = alarm_get_string_for_kind(kind, all_caps);
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(cell_text, &data->day_picker_window), NULL, NULL);
}

static void prv_day_picker_handle_selection(MenuLayer *menu_layer, MenuIndex *cell_index,
                                            void *callback_context) {
  AlarmEditorData *data = (AlarmEditorData *)callback_context;
  data->day_picker_was_completed = false;

  if (cell_index->row == DayPickerMenuItemsCustom) {
    prv_handle_custom_day_selection(cell_index->row, callback_context);
  } else {
    data->day_picker_was_completed = true;
    prv_handle_selection(cell_index->row, callback_context);
  }
}

static void prv_setup_day_picker_window(AlarmEditorData *data) {
  window_init(&data->day_picker_window, WINDOW_NAME("Alarm Day Picker"));
  window_set_user_data(&data->day_picker_window, data);
  data->day_picker_window.window_handlers.unload = prv_day_picker_window_unload;

  GRect bounds = data->day_picker_window.layer.bounds;
#if PBL_ROUND
  bounds = grect_inset_internal(bounds, 0, STATUS_BAR_LAYER_HEIGHT);
#endif
  menu_layer_init(&data->day_picker_menu_layer, &bounds);
  menu_layer_set_callbacks(&data->day_picker_menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_sections = prv_day_picker_get_num_sections,
    .get_num_rows = prv_day_picker_get_num_rows,
    .get_cell_height = prv_day_picker_get_cell_height,
    .draw_row = prv_day_picker_draw_row,
    .select_click = prv_day_picker_handle_selection,
  });
  menu_layer_set_highlight_colors(&data->day_picker_menu_layer,
                                  ALARMS_APP_HIGHLIGHT_COLOR,
                                  GColorWhite);
  menu_layer_set_click_config_onto_window(&data->day_picker_menu_layer, &data->day_picker_window);
  layer_add_child(&data->day_picker_window.layer,
                  menu_layer_get_layer(&data->day_picker_menu_layer));
  if (!alarm_get_kind(data->alarm_id, &data->alarm_kind)) {
    data->alarm_kind = ALARM_KIND_JUST_ONCE;
  }
  menu_layer_set_selected_index(&data->day_picker_menu_layer,
                                (MenuIndex) { 0, prv_alarm_kind_to_index(data->alarm_kind) },
                                MenuRowAlignCenter, false);
}

static uint16_t prv_custom_day_picker_get_num_sections(struct MenuLayer *menu_layer,
                                                       void *callback_context) {
  return 1;
}

static uint16_t prv_custom_day_picker_get_num_rows(struct MenuLayer *menu_layer,
                                                   uint16_t section_index, void *callback_context) {
  return DAYS_PER_WEEK + 1;
}

static int16_t prv_custom_day_picker_get_cell_height(struct MenuLayer *menu_layer,
                                                     MenuIndex *cell_index,
                                                     void *callback_context) {
  return ALARM_DAY_LIST_CELL_HEIGHT;
}

static void prv_custom_day_picker_draw_row(GContext *ctx, const Layer *cell_layer,
                                           MenuIndex *cell_index, void *callback_context) {
  AlarmEditorData *data = (AlarmEditorData *)callback_context;
  GBitmap *ptr_bitmap;

  if (cell_index->row == 0) { // "completed selection" row
    GRect box;
    uint32_t new_resource_id = RESOURCE_ID_CHECKMARK_ICON_BLACK;

    if (!prv_is_custom_day_scheduled(data)) {
      // no days selected
      if (menu_cell_layer_is_highlighted(cell_layer)) {
        if (data->show_check_something_first_text) { // clicking "complete" when no days selected
          box.size = GSize(cell_layer->bounds.size.w, ALARM_DAY_LIST_CELL_HEIGHT);
          box.origin = GPoint(0, 4);
          graphics_draw_text(ctx, i18n_get("Check something first.",
                                           &data->custom_day_picker_window),
                             fonts_get_system_font(FONT_KEY_GOTHIC_18), box, GTextOverflowModeFill,
                             GTextAlignmentCenter, NULL);
          return;
        } else { // row highlighted and no days selected
          new_resource_id = RESOURCE_ID_CHECKMARK_ICON_DOTTED;
        }
      }
    }

    if (new_resource_id != data->current_checkmark_icon_resource_id) {
      data->current_checkmark_icon_resource_id = new_resource_id;
      gbitmap_deinit(&data->checkmark_icon);
      gbitmap_init_with_resource(&data->checkmark_icon, data->current_checkmark_icon_resource_id);
    }

    box.origin = GPoint((((cell_layer->bounds.size.w)/2)-((data->checkmark_icon.bounds.size.w)/2)),
                        (((cell_layer->bounds.size.h)/2)-((data->checkmark_icon.bounds.size.h)/2)));
    box.size = data->checkmark_icon.bounds.size;
    graphics_context_set_compositing_mode(ctx, GCompOpTint);
    graphics_draw_bitmap_in_rect(ctx, &data->checkmark_icon, &box);

  } else { // drawing a day of the week
    const char *cell_text;
    // List should start off with Monday
    uint16_t day_index = cell_index->row % DAYS_PER_WEEK;
    const struct lc_time_T *time_locale = time_locale_get();
    cell_text = i18n_get(time_locale->weekday[day_index], &data->custom_day_picker_window);

    if (data->scheduled_days[(cell_index->row) % DAYS_PER_WEEK]) {
      ptr_bitmap = &data->selected_icon;
    } else {
      ptr_bitmap = &data->deselected_icon;
    }
    graphics_context_set_compositing_mode(ctx, GCompOpTint);
    menu_cell_basic_draw_icon_right(ctx, cell_layer, cell_text, NULL, ptr_bitmap);
  }
}

static bool prv_is_custom_day_scheduled(AlarmEditorData *data) {
  for (unsigned int i = 0; i < sizeof(data->scheduled_days); i++) {
    if (data->scheduled_days[i]) {
      return true;
    }
  }
  return false;
}

static void prv_custom_day_picker_handle_selection(MenuLayer *menu_layer, MenuIndex *cell_index,
                                                   void *callback_context) {
  AlarmEditorData *data = (AlarmEditorData *)callback_context;

  if (cell_index->row == 0) { // selected the "completed day selection" row
    if (!prv_is_custom_day_scheduled(data)) { // clicking "complete" when no days are selected
      data->show_check_something_first_text = true;
      layer_mark_dirty(menu_layer_get_layer(menu_layer));
    } else {
      data->custom_day_picker_was_completed = true;
      if (data->creating_alarm) {
        const AlarmInfo info = {
          .hour = data->alarm_hour,
          .minute = data->alarm_minute,
          .kind = ALARM_KIND_CUSTOM,
          .scheduled_days = &data->scheduled_days,
          .is_smart = (data->alarm_type == AlarmType_Smart),
        };
        data->alarm_id = alarm_create(&info);
        data->complete_callback(CREATED, data->alarm_id, data->callback_context);
      } else {
        alarm_set_custom(data->alarm_id, data->scheduled_days);
        data->complete_callback(EDITED, data->alarm_id, data->callback_context);
      }
      app_window_stack_pop(true);
    }
  } else { // selecting a day of the week
    // day_of_week index starts from sunday, and printed list starts from monday
    uint16_t day_of_week = (cell_index->row) % DAYS_PER_WEEK;
    data->scheduled_days[day_of_week] = !data->scheduled_days[day_of_week]; // toggle selection
    layer_mark_dirty(menu_layer_get_layer(menu_layer));
  }
}

static void prv_custom_day_picker_selection_changed(MenuLayer *menu_layer, MenuIndex new_index,
                                                    MenuIndex old_index, void *callback_context) {
  AlarmEditorData *data = (AlarmEditorData*) callback_context;
  if (old_index.row == 0) {
    data->show_check_something_first_text = false;
  }
}

static void prv_setup_custom_day_picker_window(AlarmEditorData *data) {
  window_init(&data->custom_day_picker_window, WINDOW_NAME("Alarm Custom Day Picker"));
  window_set_user_data(&data->custom_day_picker_window, data);
  data->custom_day_picker_window.window_handlers.unload = prv_custom_day_picker_window_unload;

  GRect bounds = data->custom_day_picker_window.layer.bounds;
#if PBL_ROUND
  bounds = grect_inset_internal(bounds, 0, STATUS_BAR_LAYER_HEIGHT);
#endif
  menu_layer_init(&data->custom_day_picker_menu_layer, &bounds);
  menu_layer_set_callbacks(&data->custom_day_picker_menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_sections = prv_custom_day_picker_get_num_sections,
    .get_num_rows = prv_custom_day_picker_get_num_rows,
    .get_cell_height = prv_custom_day_picker_get_cell_height,
    .draw_row = prv_custom_day_picker_draw_row,
    .select_click = prv_custom_day_picker_handle_selection,
    .selection_changed = prv_custom_day_picker_selection_changed
  });
  menu_layer_set_highlight_colors(&data->custom_day_picker_menu_layer,
                                  ALARMS_APP_HIGHLIGHT_COLOR,
                                  GColorWhite);
  menu_layer_set_click_config_onto_window(&data->custom_day_picker_menu_layer,
                                          &data->custom_day_picker_window);
  layer_add_child(&data->custom_day_picker_window.layer,
                  menu_layer_get_layer(&data->custom_day_picker_menu_layer));
  gbitmap_init_with_resource(&data->selected_icon, RESOURCE_ID_CHECKBOX_ICON_CHECKED);
  gbitmap_init_with_resource(&data->deselected_icon, RESOURCE_ID_CHECKBOX_ICON_UNCHECKED);
  gbitmap_init_with_resource(&data->checkmark_icon, RESOURCE_ID_CHECKMARK_ICON_BLACK);
  data->current_checkmark_icon_resource_id = RESOURCE_ID_CHECKMARK_ICON_BLACK;
  app_window_stack_push(&data->custom_day_picker_window, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Time Picker

static void prv_time_picker_window_unload(Window *window) {
  AlarmEditorData *data = (AlarmEditorData *)window_get_user_data(window);
  if (data->creating_alarm) {
#if !CAPABILITY_HAS_HEALTH_TRACKING
    prv_call_complete_cancelled_if_no_alarm(data);
#endif
    return;
  }

  // Editing time
  time_selection_window_deinit(&data->time_picker_window);

  if (data->time_picker_was_completed) {
    data->complete_callback(EDITED, data->alarm_id, data->callback_context);
  }
  i18n_free_all(data);
  task_free(data);
  data = NULL;
}

static void prv_time_picker_window_appear(Window *window) {
  AlarmEditorData *data = (AlarmEditorData *)window_get_user_data(window);
  const bool is_smart = (data->alarm_type == AlarmType_Smart);
  const char *label = (!data->creating_alarm ? i18n_noop("Change Time") :
                       is_smart ? i18n_noop("New Smart Alarm") : i18n_noop("New Alarm"));
  /// Displays as "Wake up between" then "8:00 AM - 8:30 AM" below on a separate line
  const char *range_text = PBL_IF_RECT_ELSE(i18n_noop("Wake up between"),
  /// Displays as "8:00 AM - 8:30 AM" then "Wake up interval" below on a separate line
                                            i18n_noop("Wake up interval"));
  const TimeSelectionWindowConfig config = {
    .label = i18n_get(label, data),
    .range = {
      .update = true,
      .text = is_smart ? i18n_get(range_text, data) : NULL,
      .duration_m = SMART_ALARM_RANGE_S / SECONDS_PER_MINUTE,
      .enabled = is_smart,
    },
  };
  time_selection_window_configure(&data->time_picker_window, &config);
  // Reset the selection layer to the first cell
  data->time_picker_window.selection_layer.selected_cell_idx = 0;
}

static void prv_time_picker_complete(TimeSelectionWindowData *time_picker_window, void *cb_data) {
  AlarmEditorData *data = (AlarmEditorData *) cb_data;
  data->time_picker_was_completed = true;
  data->alarm_hour = time_picker_window->time_data.hour;
  data->alarm_minute = time_picker_window->time_data.minute;

  if (data->creating_alarm) {
    app_window_stack_push(&data->day_picker_window, true);
  } else {
    alarm_set_time(data->alarm_id, data->alarm_hour, data->alarm_minute);
    app_window_stack_remove(&time_picker_window->window, true);
  }
}

static void prv_setup_time_picker_window(AlarmEditorData *data) {
  const TimeSelectionWindowConfig config = {
    .color = ALARMS_APP_HIGHLIGHT_COLOR,
    .callback = {
      .update = true,
      .complete = prv_time_picker_complete,
      .context = data,
    },
  };
  time_selection_window_init(&data->time_picker_window, &config);
  window_set_user_data(&data->time_picker_window.window, data);
  data->time_picker_window.window.window_handlers.unload = prv_time_picker_window_unload;
  data->time_picker_window.window.window_handlers.appear = prv_time_picker_window_appear;

  if (data->creating_alarm) {
    time_selection_window_set_to_current_time(&data->time_picker_window);
  } else {
    int hour, minute;
    alarm_get_hours_minutes(data->alarm_id, &hour, &minute);
    data->time_picker_window.time_data.hour = hour;
    data->time_picker_window.time_data.minute = minute;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Type Picker

static void prv_type_menu_unload(OptionMenu *option_menu, void *context) {
  AlarmEditorData *data = settings_option_menu_get_context(context);
  prv_call_complete_cancelled_if_no_alarm(data);
  data->alarm_type_menu = NULL;
}

static void prv_type_menu_select(OptionMenu *option_menu, int selection, void *context) {
  AlarmEditorData *data = settings_option_menu_get_context(context);
  data->alarm_type = selection;

#if CAPABILITY_HAS_HEALTH_TRACKING
  if (selection == AlarmType_Smart && !activity_prefs_tracking_is_enabled()) {
    // Notify about Health and keep the menu open
    health_tracking_ui_feature_show_disabled();
    return;
  }
#endif

  if (data->creating_alarm) {
    app_window_stack_push(&data->time_picker_window.window, true);
  } else {
    alarm_set_smart(data->alarm_id, (data->alarm_type == AlarmType_Smart));
    app_window_stack_remove(&option_menu->window, true);
  }
}

static void prv_setup_type_menu_window(AlarmEditorData *data) {
  const OptionMenuCallbacks callbacks = {
    .select = prv_type_menu_select,
    .unload = prv_type_menu_unload,
  };
  static const char *s_type_labels[AlarmTypeCount] = {
    [AlarmType_Basic] = i18n_noop("Basic Alarm"),
    [AlarmType_Smart] = i18n_noop("Smart Alarm"),
  };
  const char *title = i18n_get("New Alarm", data);
  OptionMenu *option_menu = settings_option_menu_create(
      title, OptionMenuContentType_Default, 0, &callbacks, ARRAY_LENGTH(s_type_labels),
      false /* icons_enabled */, s_type_labels, data);
  PBL_ASSERTN(option_menu);
  data->alarm_type_menu = option_menu;
  option_menu_set_highlight_colors(option_menu, ALARMS_APP_HIGHLIGHT_COLOR, GColorWhite);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Public API

Window* alarm_editor_create_new_alarm(AlarmEditorCompleteCallback complete_callback,
                                      void *callback_context) {
  AlarmEditorData* data = task_malloc_check(sizeof(AlarmEditorData));
  *data = (AlarmEditorData) {
    .alarm_id = ALARM_INVALID_ID,
    .complete_callback = complete_callback,
    .callback_context = callback_context,
    .creating_alarm = true,
  };

  // Setup the windows
  prv_setup_time_picker_window(data);
  prv_setup_day_picker_window(data);
#if CAPABILITY_HAS_HEALTH_TRACKING
  prv_setup_type_menu_window(data);
  return &data->alarm_type_menu->window;
#else
  return &data->time_picker_window.window;
#endif
}

void alarm_editor_update_alarm_time(AlarmId alarm_id, AlarmType alarm_type,
                                    AlarmEditorCompleteCallback complete_callback,
                                    void *callback_context) {
  AlarmEditorData* data = task_malloc_check(sizeof(AlarmEditorData));
  *data = (AlarmEditorData) {
    .alarm_id = alarm_id,
    .alarm_type = alarm_type,
    .complete_callback = complete_callback,
    .callback_context = callback_context,
  };

  prv_setup_time_picker_window(data);

  app_window_stack_push(&data->time_picker_window.window, true);
}

void alarm_editor_update_alarm_days(AlarmId alarm_id, AlarmEditorCompleteCallback complete_callback,
                                    void *callback_context) {
  AlarmEditorData* data = task_malloc_check(sizeof(AlarmEditorData));
  *data = (AlarmEditorData) {
    .alarm_id = alarm_id,
    .complete_callback = complete_callback,
    .callback_context = callback_context,
  };
  alarm_get_kind(alarm_id, &data->alarm_kind);
  if (data->alarm_kind == ALARM_KIND_CUSTOM) {
    alarm_get_custom_days(alarm_id, data->scheduled_days);
  }

  prv_setup_day_picker_window(data);

  app_window_stack_push(&data->day_picker_window, true);
}
