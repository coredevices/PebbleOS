/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "speaker_volume_window.h"

#ifdef CONFIG_SPEAKER

#include "menu.h"

#include "applib/ui/app_window_stack.h"
#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "pbl/services/speaker/speaker_service.h"

#define VOLUME_STEP 5

static void prv_play_preview(int value) {
  // Restart the preview on every step so rapid changes aren't rejected as same-priority playback.
  speaker_service_stop_for_task(PebbleTask_App);
  speaker_service_set_owner_task(PebbleTask_App);
  speaker_service_play_volume_preview((uint8_t)value);
}

static void prv_save(int value) {
  alerts_preferences_set_speaker_volume((uint8_t)value);
  speaker_service_handle_audio_prefs_changed();
  settings_menu_mark_dirty(SettingsMenuItemVibrations);
}

#ifdef CONFIG_TOUCH

#include "applib/ui/value_picker_window.h"

typedef struct SpeakerVolumeWindowData {
  ValuePickerWindow picker_window;
} SpeakerVolumeWindowData;

static void prv_changed(ValuePickerWindow *picker_window, void *context) {
  prv_play_preview(value_picker_window_get_value(picker_window));
}

static void prv_selected(ValuePickerWindow *picker_window, void *context) {
  prv_save(value_picker_window_get_value(picker_window));
  app_window_stack_remove(&picker_window->window, true /* animated */);
}

static void prv_unload(ValuePickerWindow *picker_window, void *context) {
  speaker_service_stop_for_task(PebbleTask_App);
  i18n_free_all(context);
  app_free(context);
}

void speaker_volume_window_push(void) {
  SpeakerVolumeWindowData *data = app_zalloc_check(sizeof(*data));
  const ValuePickerContent content = {
    .title = i18n_get("Volume", data),
    .unit = "%",
    .value = alerts_preferences_get_speaker_volume(),
    .min_value = 0,
    .max_value = 100,
    .step = VOLUME_STEP,
  };
  value_picker_window_init(&data->picker_window, &content, NULL,
                           (ValuePickerWindowCallbacks){
                             .changed = prv_changed,
                             .selected = prv_selected,
                             .unload = prv_unload,
                           },
                           data);
  app_window_stack_push(&data->picker_window.window, true /* animated */);
}

#else

#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/ui/system_icons.h"

#include <stddef.h>
#include <stdio.h>

#define BUTTON_REPEAT_INTERVAL_MS 100

typedef struct SpeakerVolumeWindowData {
  Window window; //!< Must be first: the base layer is cast back to this struct.
  ActionBarLayer action_bar;
  int16_t value;
} SpeakerVolumeWindowData;

static void prv_update_proc(Layer *layer, GContext *ctx) {
  _Static_assert(offsetof(Window, layer) == 0, "");
  _Static_assert(offsetof(SpeakerVolumeWindowData, window) == 0, "");
  SpeakerVolumeWindowData *data = (SpeakerVolumeWindowData *)layer;

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &layer->bounds);
  graphics_context_set_text_color(ctx, GColorBlack);

  const int16_t x_margin = 5;
  const GRect content =
      grect_inset(layer->bounds, GEdgeInsets(0, ACTION_BAR_WIDTH + x_margin, 0, x_margin));

  GRect title_frame = content;
  title_frame.origin.y = PBL_IF_ROUND_ELSE(24, 16);
  title_frame.size.h = 30;
  graphics_draw_text(ctx, i18n_get("Volume", data), fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     title_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char value_text[8];
  snprintf(value_text, sizeof(value_text), "%d%%", (int)data->value);

  GFont value_font = fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS);
  const int16_t value_height = fonts_get_font_height(value_font);
  GRect value_frame = content;
  value_frame.origin.y = (layer->bounds.size.h - value_height) / 2;
  value_frame.size.h = value_height + 4; // slack so nothing clips
  graphics_draw_text(ctx, value_text, value_font, value_frame, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void prv_set_value(SpeakerVolumeWindowData *data, int new_value) {
  if (new_value < 0) {
    new_value = 0;
  } else if (new_value > 100) {
    new_value = 100;
  }
  if (new_value == data->value) {
    return;
  }
  data->value = (int16_t)new_value;
  layer_mark_dirty(&data->window.layer);
  prv_play_preview(data->value);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, SpeakerVolumeWindowData *data) {
  prv_set_value(data, data->value + VOLUME_STEP);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, SpeakerVolumeWindowData *data) {
  prv_set_value(data, data->value - VOLUME_STEP);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, SpeakerVolumeWindowData *data) {
  prv_save(data->value);
  app_window_stack_remove(&data->window, true /* animated */);
}

static void prv_click_config_provider(SpeakerVolumeWindowData *data) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_REPEAT_INTERVAL_MS,
                                          (ClickHandler)prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_REPEAT_INTERVAL_MS,
                                          (ClickHandler)prv_down_click_handler);
  // Multi-click setup so the action bar's inverted segment stays visible for a
  // moment as press feedback (same work-around as NumberWindow).
  window_multi_click_subscribe(BUTTON_ID_SELECT, 1, 2, 25, true,
                               (ClickHandler)prv_select_click_handler);
}

static void prv_window_load(Window *window) {
  SpeakerVolumeWindowData *data = (SpeakerVolumeWindowData *)window;
  ActionBarLayer *action_bar = &data->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_set_context(action_bar, data);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, &s_bar_icon_up_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, &s_bar_icon_down_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, &s_bar_icon_check_bitmap);
  action_bar_layer_set_click_config_provider(action_bar,
                                             (ClickConfigProvider)prv_click_config_provider);
  action_bar_layer_add_to_window(action_bar, window);
}

static void prv_window_unload(Window *window) {
  SpeakerVolumeWindowData *data = (SpeakerVolumeWindowData *)window;
  // Stop any in-flight preview tone.
  speaker_service_stop_for_task(PebbleTask_App);
  action_bar_layer_deinit(&data->action_bar);
  i18n_free_all(data);
  app_free(data);
}

void speaker_volume_window_push(void) {
  SpeakerVolumeWindowData *data = app_zalloc_check(sizeof(*data));
  data->value = alerts_preferences_get_speaker_volume();

  window_init(&data->window, WINDOW_NAME("Speaker Volume"));
  window_set_window_handlers(&data->window, &(WindowHandlers){
                                              .load = prv_window_load,
                                              .unload = prv_window_unload,
                                            });
  layer_set_update_proc(&data->window.layer, prv_update_proc);

  app_window_stack_push(&data->window, true /* animated */);
}

#endif // CONFIG_TOUCH

#endif // CONFIG_SPEAKER
