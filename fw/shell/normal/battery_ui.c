/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "battery_ui.h"

#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/time.h>
#include <pbl/services/battery/battery_curve.h>
#include <pbl/services/clock.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <applib/applib_malloc.auto.h>
#include <applib/graphics/gdraw_command_list.h>
#include <applib/graphics/gpath.h>
#include <applib/ui/dialogs/dialog_private.h>
#include <applib/ui/dialogs/simple_dialog.h>
#include <applib/ui/kino/kino_reel_custom.h>
#include <applib/ui/ui.h>
#include <kernel/ui/kernel_ui.h>
#include <kernel/ui/modals/modal_manager.h>
#include <resource/resource_ids.auto.h>

typedef void (*DialogUpdateFn)(Dialog *, void *);

static Dialog *s_dialog = nullptr;

// Charging icon: a static cup with the level bar, the drips and the steam wisp drawn on top. The
// bar rises in up to nine even stages, stopping at the charge level (the last stage ends with the
// steam wisp), then drains back to the bottom. Coordinates are in 1/8 px.
#define CHARGE_STAGES         9
#define CHARGE_STAGE_PERCENT  10
#define CHARGE_FRAME_MS       35
#define CHARGE_STAGE_FRAMES   40
#define CHARGE_DRAIN_FRAMES   11
#define CHARGE_STEAM_FRAME    20
#define CHARGE_RIPPLE_RISE    64
#define CHARGE_RIPPLE_MAX     8
#define CHARGE_BAR_WIDTH      4
#define CHARGE_BAR_CENTER_X   484
#define CHARGE_BAR_EMPTY_Y    564
#define CHARGE_BAR_FULL_Y     352
#define CHARGE_BAR_EMPTY_HALF 40
#define CHARGE_BAR_FULL_HALF  57
#define CHARGE_STEAM_WIDTH    3
#define CHARGE_STEAM_X        464
#define CHARGE_STEAM_Y        40
#define CHARGE_STEAM_POINTS   4

typedef struct {
  KinoReel *reel;
  uint32_t elapsed_ms;
  uint32_t intro_ms;
  uint32_t cycle_start;
  uint32_t cycle_frames;
  int32_t frame;
  uint8_t percent;
} ChargeReelData;

typedef struct {
  uint8_t num_points;
  uint8_t points[CHARGE_STEAM_POINTS][2];
} ChargeSteamFrame;

//! Bar ends (left, right) along a stage, relative to where it settles. Offsets above
//! CHARGE_RIPPLE_MAX are the rise, drawn for a rise of CHARGE_RIPPLE_RISE.
static const int8_t s_charge_bar_ripple[CHARGE_STAGE_FRAMES][2] = {
  {64, 64},  {64, 64}, {64, 64},   {63, 64},   {62, 63},   {61, 62},   {60, 60},   {60, 30},
  {39, -7},  {1, -20}, {-20, -17}, {-22, -13}, {-21, -12}, {-19, -11}, {-4, -14},  {0, -4},
  {2, 0},    {5, 2},   {6, 5},     {6, 6},     {6, 6},     {4, 6},     {3, 4},     {0, 3},
  {-2, 0},   {-5, -2}, {-7, -5},   {-8, -7},   {-10, -8},  {-10, -10}, {-10, -10}, {-10, -10},
  {-9, -10}, {-8, -9}, {-6, -8},   {-5, -6},   {-3, -5},   {-2, -3},   {-1, -2},   {0, -1},
};

//! Bar while it drains, relative to the bottom
static const int8_t s_charge_bar_drain[CHARGE_DRAIN_FRAMES] = {24, 18, 14, 10, 6, 2,
                                                               0,  0,  -1, -2, -2};

//! Drips (top, bottom) falling into the cup from the second frame of each stage
static const int16_t s_charge_drips[][3][2] = {
  {{-20, 12}},
  {{2, 34}, {76, 108}},
  {{8, 40}, {98, 130}, {172, 204}},
  {{11, 43}, {104, 136}, {194, 226}},
  {{12, 44}, {107, 139}, {200, 232}},
  {{16, 48}, {112, 144}, {208, 240}},
  {{52, 84}, {148, 180}, {244, 276}},
  {{154, 186}},
  {{242, 274}},
  {{274, 282}},
};

//! Steam wisp at the end of the last stage, relative to (CHARGE_STEAM_X, CHARGE_STEAM_Y)
static const ChargeSteamFrame s_charge_steam[CHARGE_STAGE_FRAMES - CHARGE_STEAM_FRAME] = {
  {2, {{44, 208}, {32, 232}}},
  {2, {{47, 170}, {27, 214}}},
  {3, {{38, 150}, {44, 158}, {25, 201}}},
  {3, {{22, 190}, {42, 146}, {25, 130}}},
  {3, {{20, 179}, {40, 135}, {8, 111}}},
  {4, {{19, 173}, {39, 129}, {7, 105}, {14, 88}}},
  {4, {{18, 166}, {38, 123}, {6, 98}, {20, 68}}},
  {4, {{17, 159}, {36, 115}, {5, 91}, {28, 51}}},
  {4, {{30, 38}, {4, 85}, {36, 110}, {17, 151}}},
  {4, {{19, 141}, {35, 106}, {4, 81}, {32, 29}}},
  {4, {{22, 130}, {35, 102}, {3, 78}, {33, 22}}},
  {4, {{28, 116}, {35, 99}, {3, 74}, {32, 19}}},
  {4, {{32, 17}, {3, 71}, {31, 94}, {29, 102}}},
  {3, {{25, 87}, {3, 67}, {32, 15}}},
  {3, {{33, 13}, {3, 65}, {10, 72}}},
  {2, {{33, 10}, {7, 56}}},
  {2, {{33, 8}, {15, 40}}},
  {2, {{33, 6}, {22, 27}}},
  {2, {{28, 16}, {33, 5}}},
  {2, {{34, 4}, {33, 7}}},
};

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

static uint32_t prv_charge_cycle_frames(uint8_t percent) {
  return prv_charge_stages(percent) * CHARGE_STAGE_FRAMES + CHARGE_DRAIN_FRAMES;
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

static int16_t prv_charge_ripple_y(int32_t offset, int16_t from_y, int16_t to_y) {
  if (offset > CHARGE_RIPPLE_MAX) {
    offset = offset * (from_y - to_y) / CHARGE_RIPPLE_RISE;
  }
  return to_y + offset;
}

static void prv_charge_draw_path(GContext *ctx, GPoint origin, GPointPrecise *points,
                                 size_t num_points) {
  for (size_t i = 0; i < num_points; i++) {
    points[i].x.raw_value += origin.x * GPOINT_PRECISE_FACTOR;
    points[i].y.raw_value += origin.y * GPOINT_PRECISE_FACTOR;
  }
  gpath_draw_outline_precise_internal(ctx, points, num_points, true);
}

static int16_t prv_charge_bar_half_width(int16_t y) {
  return CHARGE_BAR_EMPTY_HALF + (CHARGE_BAR_FULL_HALF - CHARGE_BAR_EMPTY_HALF) *
                                     (CHARGE_BAR_EMPTY_Y - y) /
                                     (CHARGE_BAR_EMPTY_Y - CHARGE_BAR_FULL_Y);
}

//! Draw the bar across the cup, its ends following the cup walls
static void prv_charge_draw_bar(GContext *ctx, GPoint origin, int16_t left_y, int16_t right_y) {
  GPointPrecise points[] = {
    GPointPrecise(CHARGE_BAR_CENTER_X - prv_charge_bar_half_width(left_y), left_y),
    GPointPrecise(CHARGE_BAR_CENTER_X + prv_charge_bar_half_width(right_y), right_y),
  };
  prv_charge_draw_path(ctx, origin, points, ARRAY_LENGTH(points));
}

static void prv_charge_draw_drips(GContext *ctx, GPoint origin, uint32_t frame) {
  if (frame < 1 || frame > ARRAY_LENGTH(s_charge_drips)) {
    return;
  }

  for (size_t i = 0; i < ARRAY_LENGTH(s_charge_drips[0]); i++) {
    const int16_t *drip = s_charge_drips[frame - 1][i];
    if (drip[0] == drip[1]) {
      continue;
    }
    GPointPrecise points[] = {
      GPointPrecise(CHARGE_BAR_CENTER_X, drip[0]),
      GPointPrecise(CHARGE_BAR_CENTER_X, drip[1]),
    };
    prv_charge_draw_path(ctx, origin, points, ARRAY_LENGTH(points));
  }
}

static void prv_charge_draw_steam(GContext *ctx, GPoint origin, uint32_t frame) {
  if (frame < CHARGE_STEAM_FRAME) {
    return;
  }

  const ChargeSteamFrame *steam = &s_charge_steam[frame - CHARGE_STEAM_FRAME];
  GPointPrecise points[CHARGE_STEAM_POINTS];
  for (uint8_t i = 0; i < steam->num_points; i++) {
    points[i] =
        GPointPrecise(CHARGE_STEAM_X + steam->points[i][0], CHARGE_STEAM_Y + steam->points[i][1]);
  }
  graphics_context_set_stroke_width(ctx, CHARGE_STEAM_WIDTH);
  prv_charge_draw_path(ctx, origin, points, steam->num_points);
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
  data->elapsed_ms = elapsed_ms;
  bool changed = kino_reel_set_elapsed(data->reel, MIN(elapsed_ms, data->intro_ms));

  // The icon's intro transform plays first, without the animated parts
  if (elapsed_ms < data->intro_ms) {
    changed |= (data->frame >= 0);
    data->frame = -1;
    return changed;
  }

  const uint32_t frame = (elapsed_ms - data->intro_ms) / CHARGE_FRAME_MS;
  if (data->frame < 0 || frame < data->cycle_start) {
    data->cycle_start = 0;
    data->percent = s_charge_percent;
    data->cycle_frames = prv_charge_cycle_frames(data->percent);
  }

  // Charge level changes are picked up at the start of a cycle
  while (frame - data->cycle_start >= data->cycle_frames) {
    data->cycle_start += data->cycle_frames;
    data->percent = s_charge_percent;
    data->cycle_frames = prv_charge_cycle_frames(data->percent);
  }

  const int32_t cycle_frame = frame - data->cycle_start;
  changed |= (cycle_frame != data->frame);
  data->frame = cycle_frame;
  return changed;
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
  kino_reel_draw_processed(data->reel, ctx, offset, processor);
  if (data->frame < 0) {
    return;
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, CHARGE_BAR_WIDTH);

  const uint32_t stages = prv_charge_stages(data->percent);
  const uint32_t rise_frames = stages * CHARGE_STAGE_FRAMES;
  if ((uint32_t)data->frame >= rise_frames) {
    const int16_t y = CHARGE_BAR_EMPTY_Y + s_charge_bar_drain[data->frame - rise_frames];
    prv_charge_draw_bar(ctx, offset, y, y);
    return;
  }

  const int32_t stage = data->frame / CHARGE_STAGE_FRAMES;
  const uint32_t frame = data->frame % CHARGE_STAGE_FRAMES;
  const int16_t from_y =
      (stage > 0) ? prv_charge_bar_stage_y(data->percent, stage - 1) : CHARGE_BAR_EMPTY_Y;
  const int16_t to_y = prv_charge_bar_stage_y(data->percent, stage);
  prv_charge_draw_bar(ctx, offset, prv_charge_ripple_y(s_charge_bar_ripple[frame][0], from_y, to_y),
                      prv_charge_ripple_y(s_charge_bar_ripple[frame][1], from_y, to_y));
  prv_charge_draw_drips(ctx, offset, frame);
  if (stage == CHARGE_STAGES - 1) {
    prv_charge_draw_steam(ctx, offset, frame);
  }
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

//! Wrap the static charging icon to draw the animated parts on top
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
    .intro_ms = kino_reel_get_duration(reel),
    .frame = -1,
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
  const char *message = clock_get_relative_daypart_string(pbl_realtime(), battery_hours_left);

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
