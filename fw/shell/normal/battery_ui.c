/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "battery_ui.h"

#include <stdint.h>
#include <stdio.h>

#include <pbl/services/battery/battery_curve.h>
#include <pbl/services/clock.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/math.h>

#include <applib/graphics/gdraw_command.h>
#include <applib/graphics/gdraw_command_list.h>
#include <applib/ui/dialogs/dialog_private.h>
#include <applib/ui/dialogs/simple_dialog.h>
#include <applib/ui/ui.h>
#include <kernel/ui/kernel_ui.h>
#include <kernel/ui/modals/modal_manager.h>
#include <resource/resource_ids.auto.h>

typedef void (*DialogUpdateFn)(Dialog *, void *);

static Dialog *s_dialog = nullptr;

// Liquid polygon in the charging icon: bottom-left, top-left, top-right, bottom-right at 100%
#define LIQUID_COMMAND_IDX 2
#define LIQUID_RISE_MS     2500
#define LIQUID_CYCLE_MS    3200

typedef struct {
  GDrawCommandProcessor processor;
  Animation *animation;
  uint8_t percent;
  uint8_t level;
} LiquidProcessor;

typedef struct {
  uint32_t percent;
  GColor background_color;
  ResourceId warning_icon;
} BatteryWarningDisplayData;

// UI Callbacks
///////////////////////

static const GColor s_warning_color[] = {
  {.argb = GColorLightGrayARGB8},
  {.argb = GColorRedARGB8},
};

static const ResourceId s_warning_icon[] = {
  RESOURCE_ID_BATTERY_ICON_LOW_LARGE, RESOURCE_ID_BATTERY_ICON_VERY_LOW_LARGE
};

static GPoint prv_lerp_point(GPoint from, GPoint to, uint8_t percent) {
  return GPoint(from.x + (to.x - from.x) * percent / 100, from.y + (to.y - from.y) * percent / 100);
}

static void prv_fill_liquid(GDrawCommandProcessor *processor, GDrawCommand *processed_command,
                            size_t processed_command_max_size, const GDrawCommandList *list,
                            const GDrawCommand *command) {
  if (command != gdraw_command_list_get_command((GDrawCommandList *)list, LIQUID_COMMAND_IDX)) {
    return;
  }

  const LiquidProcessor *liquid = (LiquidProcessor *)processor;
  const GPoint bottom_left = gdraw_command_get_point(processed_command, 0);
  const GPoint top_left = gdraw_command_get_point(processed_command, 1);
  const GPoint top_right = gdraw_command_get_point(processed_command, 2);
  const GPoint bottom_right = gdraw_command_get_point(processed_command, 3);

  gdraw_command_set_point(processed_command, 1,
                          prv_lerp_point(bottom_left, top_left, liquid->level));
  gdraw_command_set_point(processed_command, 2,
                          prv_lerp_point(bottom_right, top_right, liquid->level));
  gdraw_command_set_fill_color(processed_command,
                               PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorLightGray));
  gdraw_command_set_hidden(processed_command, false);
}

static LiquidProcessor s_liquid = {
  .processor.command = prv_fill_liquid,
};

static void prv_liquid_animation_update(Animation *animation, const AnimationProgress progress) {
  const uint32_t elapsed_ms = (uint32_t)progress * LIQUID_CYCLE_MS / ANIMATION_NORMALIZED_MAX;
  const uint32_t rise_ms = MIN(elapsed_ms, LIQUID_RISE_MS);
  const uint8_t level = s_liquid.percent + (100 - s_liquid.percent) * rise_ms / LIQUID_RISE_MS;

  if (level != s_liquid.level && s_dialog) {
    s_liquid.level = level;
    layer_mark_dirty(&s_dialog->icon_layer.layer);
  }
}

static const AnimationImplementation s_liquid_animation_impl = {
  .update = prv_liquid_animation_update,
};

static void prv_liquid_animation_start(void) {
  if (s_liquid.animation) {
    return;
  }

  s_liquid.level = s_liquid.percent;
  Animation *animation = animation_create();
  animation_set_implementation(animation, &s_liquid_animation_impl);
  animation_set_duration(animation, LIQUID_CYCLE_MS);
  animation_set_curve(animation, AnimationCurveLinear);
  animation_set_play_count(animation, ANIMATION_PLAY_COUNT_INFINITE);
  animation_schedule(animation);
  s_liquid.animation = animation;
}

static void prv_liquid_animation_stop(void) {
  if (s_liquid.animation) {
    animation_destroy(s_liquid.animation);
    s_liquid.animation = nullptr;
  }
}

static void prv_set_liquid_processor(Dialog *dialog) {
  if (dialog->icon_id == RESOURCE_ID_BATTERY_ICON_CHARGING_LARGE) {
    kino_layer_set_draw_command_processor(&dialog->icon_layer, &s_liquid.processor);
    prv_liquid_animation_start();
  } else {
    kino_layer_set_draw_command_processor(&dialog->icon_layer, nullptr);
    prv_liquid_animation_stop();
  }
}

static void prv_update_liquid(Dialog *dialog) {
  if (window_is_loaded(&dialog->window)) {
    prv_set_liquid_processor(dialog);
  }
}

static void prv_update_ui_fully_charged(Dialog *dialog, void *ignored) {
  dialog_set_text(dialog, i18n_get("Fully Charged", dialog));
  dialog_set_background_color(dialog, GColorKellyGreen);
  dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_FULL_LARGE);
  prv_update_liquid(dialog);
}

static void prv_update_ui_charging(Dialog *dialog, void *ignored) {
  dialog_set_text(dialog, i18n_get("Charging", dialog));
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_CHARGING_LARGE);
  prv_update_liquid(dialog);
}

static void prv_update_ui_warning(Dialog *dialog, void *context) {
  const BatteryWarningDisplayData *data = context;
  const uint32_t percent = data->percent;
  dialog_set_background_color(dialog, data->background_color);
  const size_t warning_length = 64;
  char buffer[warning_length];
  const uint32_t battery_hours_left = battery_curve_get_hours_remaining(percent);
  const char *message = clock_get_relative_daypart_string(rtc_get_time(), battery_hours_left);

  if (message) {
    snprintf(buffer, warning_length, i18n_get("Powered 'til %s", dialog),
             i18n_get(message, dialog));
    dialog_set_text(dialog, buffer);
  }

  dialog_set_icon(dialog, data->warning_icon);
  prv_update_liquid(dialog);
}

static void prv_dialog_on_load(void *context) {
  prv_set_liquid_processor(context);
}

static void prv_dialog_on_unload(void *context) {
  Dialog *dialog = context;
  i18n_free_all(dialog);
  if (dialog == s_dialog) {
    prv_liquid_animation_stop();
    s_dialog = nullptr;
  }
}

static void prv_display_modal(WindowStack *stack, DialogUpdateFn update_fn, void *data) {
  if (s_dialog) {
    update_fn(s_dialog, data);
    return;
  }

  SimpleDialog *new_simple_dialog = simple_dialog_create(WINDOW_NAME("Battery Status"));

  Dialog *new_dialog = simple_dialog_get_dialog(new_simple_dialog);
  dialog_set_callbacks(new_dialog,
                       &(DialogCallbacks){
                         .load = prv_dialog_on_load,
                         .unload = prv_dialog_on_unload,
                       },
                       nullptr);
  update_fn(new_dialog, data);

  Dialog *old_dialog = s_dialog;
  s_dialog = new_dialog;
  simple_dialog_push(new_simple_dialog, stack);

#if PBL_ROUND
  // For circular display, to fit some battery_ui messages requires 3 lines
  // Simple dialog only allows up to 2 lines, so adjust here
  // This has to occur after the dialog push has been called
  TextLayer *text_layer = &new_simple_dialog->dialog.text_layer;
  GContext *ctx = graphics_context_get_current_context();
  const int font_height = fonts_get_font_height(text_layer->font);
  const int text_cap_height = fonts_get_font_cap_offset(text_layer->font);
  const int max_text_height = 2 * font_height + text_cap_height;
  const int32_t text_height = text_layer_get_content_size(ctx, text_layer).h;
  if (text_height > max_text_height) {
    // Values used below were to improve visual aesthetics and were reviewed by design
    const int num_lines = 3;
    const int line_spacing_delta = -4;
    const int text_shift_y = -2;
    const int text_box_height =
        (font_height + text_cap_height) * num_lines + line_spacing_delta * (num_lines - 1);
    const int text_flow_inset = 6; // Modify to allow longer central lines
    text_layer_enable_screen_text_flow_and_paging(text_layer, text_flow_inset);
    text_layer_set_size(text_layer, GSize(DISP_COLS, text_box_height));
    text_layer->layer.frame.origin.y += text_shift_y;
    text_layer_set_line_spacing_delta(text_layer, line_spacing_delta);
  }
#endif

  if (old_dialog) {
    dialog_pop(old_dialog);
  }
}

// Public API
////////////////////

void battery_ui_display_plugged(uint8_t percent) {
  s_liquid.percent = percent;
  // If we're plugged in for charging, we want to alert the user of this,
  // but we don't want to overlay ourselves over anything they may have
  // on the screen at the moment.
  WindowStack *stack = modal_manager_get_window_stack(ModalPriorityGeneric);
  prv_display_modal(stack, prv_update_ui_charging, nullptr);
}

void battery_ui_update_charge_level(uint8_t percent) {
  s_liquid.percent = percent;
}

void battery_ui_display_fully_charged(void) {
  // If we're plugged in (charged), we want to alert the user of this,
  // but we don't want to overlay ourselves over anything they may have
  // on the screen at the moment.
  WindowStack *stack = modal_manager_get_window_stack(ModalPriorityGeneric);
  prv_display_modal(stack, prv_update_ui_fully_charged, nullptr);
}

void battery_ui_display_warning(uint32_t percent, BatteryUIWarningLevel warning_level) {
  // If we're not plugged in, that means we hit a critical power notification,
  // so we want to alert the user, subverting any non-critical windows they
  // have on the screen.
  WindowStack *stack = modal_manager_get_window_stack(ModalPriorityAlert);

  BatteryWarningDisplayData display_data = {
    .percent = percent,
    .background_color = s_warning_color[warning_level],
    .warning_icon = s_warning_icon[warning_level],
  };
  prv_display_modal(stack, prv_update_ui_warning, &display_data);
}

void battery_ui_dismiss_modal(void) {
  if (s_dialog) {
    dialog_pop(s_dialog);
    s_dialog = nullptr;
  }
}
