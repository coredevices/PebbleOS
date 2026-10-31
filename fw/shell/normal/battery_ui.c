/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "battery_ui.h"

#include <stdint.h>
#include <stdio.h>

#include <pbl/services/battery/battery_curve.h>
#include <pbl/services/clock.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/math.h>

#include <applib/applib_malloc.auto.h>
#include <applib/graphics/gdraw_command.h>
#include <applib/graphics/gdraw_command_list.h>
#include <applib/ui/dialogs/dialog_private.h>
#include <applib/ui/dialogs/simple_dialog.h>
#include <applib/ui/kino/kino_reel_custom.h>
#include <applib/ui/ui.h>
#include <kernel/ui/kernel_ui.h>
#include <kernel/ui/modals/modal_manager.h>
#include <resource/resource_ids.auto.h>

typedef void (*DialogUpdateFn)(Dialog *, void *);

static Dialog *s_dialog = nullptr;

// Charging icon: the level bar rises in up to nine even stages, stopping at the charge level (the
// last stage ends with the steam wisp), then drops back to the bottom. The resource has four rises,
// the middle ones are reused for the extra stages. Bar coordinates are in the resource's 1/8 px
// units.
#define CHARGE_STAGES         9
#define CHARGE_STAGE_PERCENT  10
#define CHARGE_ART_STAGES     4
#define CHARGE_STAGE_MS       1400
#define CHARGE_DROP_MS        5705
#define CHARGE_LOOP_MS        6090
#define CHARGE_BAR_CENTER_X   484
#define CHARGE_BAR_MIN_X      376
#define CHARGE_BAR_MIN_WIDTH  64
#define CHARGE_BAR_EMPTY_Y    564
#define CHARGE_BAR_FULL_Y     352
#define CHARGE_BAR_EMPTY_HALF 40
#define CHARGE_BAR_FULL_HALF  57

typedef struct {
  KinoReel *reel;
  uint32_t elapsed_ms;
  uint32_t cycle_start_ms;
  uint32_t cycle_ms;
  uint8_t percent;
} ChargeReelData;

typedef struct {
  GDrawCommandProcessor processor;
  int16_t art_from_y;
  int16_t art_to_y;
  int16_t from_y;
  int16_t to_y;
} ChargeBarProcessor;

// Where the bar settles at the end of each stage in the resource
static const int16_t s_charge_bar_art_y[CHARGE_ART_STAGES] = {504, 440, 376, 352};

static uint8_t s_charge_percent;
static KinoReel *s_charge_reel;

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

static uint32_t prv_charge_stages(uint8_t percent) {
  return CLIP(DIVIDE_CEIL(percent, CHARGE_STAGE_PERCENT), 1, CHARGE_STAGES);
}

//! The rise of the resource to play for a stage, the last one has the steam wisp
static int32_t prv_charge_art_stage(int32_t stage) {
  return (stage == CHARGE_STAGES - 1) ? (CHARGE_ART_STAGES - 1) : MIN(stage, CHARGE_ART_STAGES - 2);
}

static uint32_t prv_charge_cycle_ms(void) {
  const uint32_t stages = prv_charge_stages(s_charge_percent);
  if (stages == CHARGE_STAGES) {
    return (CHARGE_STAGES - 1) * CHARGE_STAGE_MS + CHARGE_LOOP_MS -
           (CHARGE_ART_STAGES - 1) * CHARGE_STAGE_MS;
  }

  return stages * CHARGE_STAGE_MS + (CHARGE_LOOP_MS - CHARGE_DROP_MS);
}

//! Height of the bar at the end of a stage: an even step, or the charge level if that comes first.
//! The last stage covers everything above it.
static int16_t prv_charge_bar_stage_y(uint8_t percent, int32_t stage) {
  const int32_t range = CHARGE_BAR_EMPTY_Y - CHARGE_BAR_FULL_Y;
  const int32_t level_y = CHARGE_BAR_EMPTY_Y - range * MIN(percent, 100) / 100;
  if (stage == CHARGE_STAGES - 1) {
    return level_y;
  }
  return MAX(level_y, CHARGE_BAR_EMPTY_Y - range * (stage + 1) * CHARGE_STAGE_PERCENT / 100);
}

static void prv_charge_bar_process(GDrawCommandProcessor *processor,
                                   GDrawCommand *processed_command,
                                   size_t processed_command_max_size, const GDrawCommandList *list,
                                   const GDrawCommand *command) {
  if (gdraw_command_get_num_points(processed_command) != 2) {
    return;
  }

  // The bar is the only wide two point line inside the cup
  const GPoint a = gdraw_command_get_point(processed_command, 0);
  const GPoint b = gdraw_command_get_point(processed_command, 1);
  if (MIN(a.x, b.x) < CHARGE_BAR_MIN_X || ABS(a.x - b.x) < CHARGE_BAR_MIN_WIDTH) {
    return;
  }

  const ChargeBarProcessor *bar = (const ChargeBarProcessor *)processor;

  for (uint16_t i = 0; i < 2; i++) {
    const GPoint point = (i == 0) ? a : b;
    // Keep the ripple around the stage's height, and scale the rise to it
    int32_t offset_y = point.y - bar->art_to_y;
    if (offset_y > 8) {
      offset_y = offset_y * (bar->from_y - bar->to_y) / (bar->art_from_y - bar->art_to_y);
    }
    const int16_t y = bar->to_y + offset_y;
    const int16_t half_width =
        CHARGE_BAR_EMPTY_HALF + (CHARGE_BAR_FULL_HALF - CHARGE_BAR_EMPTY_HALF) *
                                    (CHARGE_BAR_EMPTY_Y - y) /
                                    (CHARGE_BAR_EMPTY_Y - CHARGE_BAR_FULL_Y);
    const int16_t x =
        CHARGE_BAR_CENTER_X + ((point.x < CHARGE_BAR_CENTER_X) ? -half_width : half_width);
    gdraw_command_set_point(processed_command, i, GPoint(x, y));
  }
}

static void prv_charge_reel_destructor(KinoReel *reel) {
  ChargeReelData *data = kino_reel_custom_get_data(reel);
  kino_reel_destroy(data->reel);
  applib_free(data);
  s_charge_reel = nullptr;
}

static uint32_t prv_charge_reel_get_elapsed(KinoReel *reel) {
  ChargeReelData *data = kino_reel_custom_get_data(reel);
  return data->elapsed_ms;
}

static bool prv_charge_reel_set_elapsed(KinoReel *reel, uint32_t elapsed_ms) {
  ChargeReelData *data = kino_reel_custom_get_data(reel);
  if (elapsed_ms < data->cycle_start_ms) {
    data->cycle_start_ms = 0;
    data->cycle_ms = prv_charge_cycle_ms();
    data->percent = s_charge_percent;
  }

  // Charge level changes are picked up at the start of a cycle
  while (elapsed_ms - data->cycle_start_ms >= data->cycle_ms) {
    data->cycle_start_ms += data->cycle_ms;
    data->cycle_ms = prv_charge_cycle_ms();
    data->percent = s_charge_percent;
  }
  data->elapsed_ms = elapsed_ms;

  // Skip the remaining stages and jump to the drop
  const uint32_t cycle_elapsed_ms = elapsed_ms - data->cycle_start_ms;
  const uint32_t rise_ms = data->cycle_ms - (CHARGE_LOOP_MS - CHARGE_DROP_MS);
  uint32_t reel_elapsed_ms;
  if (cycle_elapsed_ms >= rise_ms) {
    reel_elapsed_ms = CHARGE_DROP_MS + cycle_elapsed_ms - rise_ms;
  } else {
    const int32_t stage = MIN(cycle_elapsed_ms / CHARGE_STAGE_MS, CHARGE_STAGES - 1);
    reel_elapsed_ms = cycle_elapsed_ms + (prv_charge_art_stage(stage) - stage) * CHARGE_STAGE_MS;
  }

  // The icon sequence loops, so later cycles play a loop ahead to not replay the intro transform
  if (data->cycle_start_ms > 0) {
    reel_elapsed_ms += CHARGE_LOOP_MS;
  }

  return kino_reel_set_elapsed(data->reel, reel_elapsed_ms);
}

static uint32_t prv_charge_reel_get_duration(KinoReel *reel) {
  return PLAY_DURATION_INFINITE;
}

static GSize prv_charge_reel_get_size(KinoReel *reel) {
  ChargeReelData *data = kino_reel_custom_get_data(reel);
  return kino_reel_get_size(data->reel);
}

static void prv_charge_reel_draw_processed(KinoReel *reel, GContext *ctx, GPoint offset,
                                           KinoReelProcessor *processor) {
  ChargeReelData *data = kino_reel_custom_get_data(reel);
  const uint32_t cycle_elapsed_ms = data->elapsed_ms - data->cycle_start_ms;
  const uint32_t rise_ms = data->cycle_ms - (CHARGE_LOOP_MS - CHARGE_DROP_MS);

  // The drop frames already end at the bottom, only the rise follows the charge level
  if (cycle_elapsed_ms >= rise_ms) {
    kino_reel_draw_processed(data->reel, ctx, offset, processor);
    return;
  }

  const int32_t stage = MIN(cycle_elapsed_ms / CHARGE_STAGE_MS, CHARGE_STAGES - 1);
  const int32_t art_stage = prv_charge_art_stage(stage);
  ChargeBarProcessor bar = {
    .processor.command = prv_charge_bar_process,
    .art_from_y = (art_stage > 0) ? s_charge_bar_art_y[art_stage - 1] : CHARGE_BAR_EMPTY_Y,
    .art_to_y = s_charge_bar_art_y[art_stage],
    .from_y = (stage > 0) ? prv_charge_bar_stage_y(data->percent, stage - 1) : CHARGE_BAR_EMPTY_Y,
    .to_y = prv_charge_bar_stage_y(data->percent, stage),
  };
  KinoReelProcessor bar_processor = {
    .draw_command_processor = &bar.processor,
  };
  kino_reel_draw_processed(data->reel, ctx, offset, &bar_processor);
}

static GDrawCommandList *prv_charge_reel_get_gdraw_command_list(KinoReel *reel) {
  ChargeReelData *data = kino_reel_custom_get_data(reel);
  return kino_reel_get_gdraw_command_list(data->reel);
}

static const KinoReelImpl s_charge_reel_impl = {
  .destructor = prv_charge_reel_destructor,
  .get_elapsed = prv_charge_reel_get_elapsed,
  .set_elapsed = prv_charge_reel_set_elapsed,
  .get_duration = prv_charge_reel_get_duration,
  .get_size = prv_charge_reel_get_size,
  .draw_processed = prv_charge_reel_draw_processed,
  .get_gdraw_command_list = prv_charge_reel_get_gdraw_command_list,
};

//! Wrap the charging icon reel so the bar drops back once it reaches the charge level
static void prv_set_charge_reel(Dialog *dialog) {
  KinoLayer *icon_layer = &dialog->icon_layer;
  KinoPlayer *player = kino_layer_get_player(icon_layer);
  KinoReel *reel = kino_layer_get_reel(icon_layer);
  if (dialog->icon_id != RESOURCE_ID_BATTERY_ICON_CHARGING_LARGE || !reel ||
      reel == s_charge_reel || !player->owns_reel) {
    return;
  }

  ChargeReelData *data = applib_malloc(sizeof(ChargeReelData));
  if (!data) {
    return;
  }

  KinoReel *charge_reel = kino_reel_custom_create(&s_charge_reel_impl, data);
  if (!charge_reel) {
    applib_free(data);
    return;
  }

  *data = (ChargeReelData){
    .reel = reel,
    .cycle_ms = prv_charge_cycle_ms(),
    .percent = s_charge_percent,
  };

  // The charge reel takes over ownership of the icon reel
  player->owns_reel = false;
  kino_layer_set_reel(icon_layer, charge_reel, true);
  s_charge_reel = charge_reel;
  kino_layer_play(icon_layer);
}

static void prv_update_ui_fully_charged(Dialog *dialog, void *ignored) {
  dialog_set_text(dialog, i18n_get("Fully Charged", dialog));
  dialog_set_background_color(dialog, GColorKellyGreen);
  dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_FULL_LARGE);
}

static void prv_update_ui_charging(Dialog *dialog, void *ignored) {
  dialog_set_text(dialog, i18n_get("Charging", dialog));
  dialog_set_background_color(dialog, GColorLightGray);
  dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_CHARGING_LARGE);
  if (window_is_loaded(&dialog->window)) {
    prv_set_charge_reel(dialog);
  }
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
}

static void prv_dialog_on_load(void *context) {
  prv_set_charge_reel(context);
}

static void prv_dialog_on_unload(void *context) {
  Dialog *dialog = context;
  i18n_free_all(dialog);
  if (dialog == s_dialog) {
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
  s_charge_percent = percent;
  // If we're plugged in for charging, we want to alert the user of this,
  // but we don't want to overlay ourselves over anything they may have
  // on the screen at the moment.
  WindowStack *stack = modal_manager_get_window_stack(ModalPriorityGeneric);
  prv_display_modal(stack, prv_update_ui_charging, nullptr);
}

void battery_ui_update_charge_level(uint8_t percent) {
  s_charge_percent = percent;
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
