/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// App-context PIN-entry window for the Security settings submenu.
// Supports two modes:
//   Set    — two-phase (enter new PIN, then re-enter to confirm)
//   Verify — single-phase (verify existing PIN against storage)
// On completion the caller-supplied callback is invoked with success/failure;
// the window pops itself on both success and user cancel (Back at pos 0).

#include "security_pin_entry.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/gcolor_definitions.h"
#include "applib/graphics/gcontext.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_circle.h"
#include "applib/graphics/text.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "board/display.h"
#include "kernel/pbl_malloc.h"
#include "popups/pin_lock/unlock_window.h"
#include "pbl/services/i18n/i18n.h"
#include "services/pin_lock/pin_lock.h"
#include "system/logging.h"
#include "system/passert.h"

// Dot layout — mirror unlock_window.c constants so the UI looks identical.
#define DOT_RADIUS    7
#define DOT_SPACING  20
#define DOT_ROW_Y   (DISP_ROWS / 2)
#define DIGIT_HALF  (DOT_RADIUS * 2)

// Title text height above the dot row.
#define TITLE_Y     (DOT_ROW_Y - DOT_RADIUS * 5)

typedef enum {
  Phase_Enter,    // collecting the first pass (or the only pass in Verify mode)
  Phase_Confirm,  // Set-mode: re-enter to confirm
} Phase;

typedef struct {
  Window window;
  PinEntry entry;
  SecurityPinEntryConfig cfg;
  Phase phase;
  uint8_t first_digits[PIN_LOCK_MAX_LEN]; // saved first-pass digits for Set-mode
} PinEntryWindowData;

static void prv_redraw(PinEntryWindowData *d) {
  layer_mark_dirty(window_get_root_layer(&d->window));
}

static void prv_pop(PinEntryWindowData *d) {
  app_window_stack_remove(&d->window, true /* animated */);
  // `d` freed in window_unload (below).
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  // Walk up to the Window from the root layer.
  Window *window = layer_get_window(layer);
  PinEntryWindowData *d = window_get_user_data(window);

  // Draw the title string.
  const char *title_key;
  if (d->cfg.mode == SecurityPinEntryMode_Verify) {
    title_key = i18n_noop("Enter PIN");
  } else if (d->phase == Phase_Confirm) {
    title_key = i18n_noop("Confirm PIN");
  } else {
    title_key = i18n_noop("Set PIN");
  }
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GRect title_box = GRect(0, TITLE_Y, DISP_COLS, 24);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n_get(title_key, d), font, title_box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Draw dots.
  const uint8_t n = d->entry.len;
  const int16_t total_w = (n - 1) * DOT_SPACING;
  const int16_t start_x = (DISP_COLS - total_w) / 2;

  for (uint8_t i = 0; i < n; i++) {
    const int16_t cx = start_x + i * DOT_SPACING;
    const GPoint centre = GPoint(cx, DOT_ROW_Y);

    if (i < d->entry.pos) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_circle(ctx, centre, DOT_RADIUS);
    } else if (i == d->entry.pos) {
      char buf[2] = { (char)('0' + d->entry.digits[i]), '\0' };
      GFont digit_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
      GRect box = GRect(cx - DIGIT_HALF, DOT_ROW_Y - DIGIT_HALF,
                        DIGIT_HALF * 2, DIGIT_HALF * 2);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, &box);
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_draw_text(ctx, buf, digit_font, box,
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_circle(ctx, centre, DOT_RADIUS);
    }
  }
}

static void prv_up_handler(ClickRecognizerRef recognizer, void *context) {
  PinEntryWindowData *d = (PinEntryWindowData *)context;
  pin_entry_up(&d->entry);
  prv_redraw(d);
}

static void prv_down_handler(ClickRecognizerRef recognizer, void *context) {
  PinEntryWindowData *d = (PinEntryWindowData *)context;
  pin_entry_down(&d->entry);
  prv_redraw(d);
}

static void prv_select_handler(ClickRecognizerRef recognizer, void *context) {
  PinEntryWindowData *d = (PinEntryWindowData *)context;

  if (!pin_entry_select(&d->entry)) {
    // Not at last digit yet; advance.
    prv_redraw(d);
    return;
  }

  // Last digit confirmed.
  if (d->cfg.mode == SecurityPinEntryMode_Verify) {
    if (pin_lock_storage_verify_pin(d->entry.digits, d->entry.len)) {
      // Capture everything before prv_pop: removal may unload+free `d`.
      const SecurityPinEntryConfig cfg = d->cfg;
      const uint8_t len = d->entry.len;
      uint8_t digits[PIN_LOCK_MAX_LEN];
      for (uint8_t i = 0; i < len; i++) {
        digits[i] = d->entry.digits[i];
      }
      prv_pop(d);
      if (cfg.on_complete) {
        cfg.on_complete(true, digits, len, cfg.ctx);
      }
    } else {
      PBL_LOG_DBG("Security PIN entry: wrong PIN, resetting");
      vibes_double_pulse();
      pin_entry_init(&d->entry, d->entry.len);
      prv_redraw(d);
    }
    return;
  }

  // SecurityPinEntryMode_Set
  if (d->phase == Phase_Enter) {
    // Save the first pass and move to confirmation.
    for (uint8_t i = 0; i < d->entry.len; i++) {
      d->first_digits[i] = d->entry.digits[i];
    }
    d->phase = Phase_Confirm;
    pin_entry_init(&d->entry, d->entry.len);
    prv_redraw(d);
  } else {
    // Compare confirmation with first pass.
    bool match = true;
    for (uint8_t i = 0; i < d->entry.len; i++) {
      if (d->entry.digits[i] != d->first_digits[i]) {
        match = false;
        break;
      }
    }
    if (match) {
      // Capture everything before prv_pop: removal may unload+free `d`.
      const SecurityPinEntryConfig cfg = d->cfg;
      const uint8_t len = d->entry.len;
      uint8_t digits[PIN_LOCK_MAX_LEN];
      for (uint8_t i = 0; i < len; i++) {
        digits[i] = d->entry.digits[i];
      }
      prv_pop(d);
      if (cfg.on_complete) {
        cfg.on_complete(true, digits, len, cfg.ctx);
      }
    } else {
      PBL_LOG_DBG("Security PIN entry: confirmation mismatch, restarting");
      vibes_double_pulse();
      d->phase = Phase_Enter;
      pin_entry_init(&d->entry, d->entry.len);
      prv_redraw(d);
    }
  }
}

static void prv_back_handler(ClickRecognizerRef recognizer, void *context) {
  PinEntryWindowData *d = (PinEntryWindowData *)context;
  if (pin_entry_back(&d->entry)) {
    // At first position: cancel and dismiss (back to Confirm = back to Enter).
    if (d->phase == Phase_Confirm) {
      d->phase = Phase_Enter;
      pin_entry_init(&d->entry, d->entry.len);
      prv_redraw(d);
    } else {
      const SecurityPinEntryConfig cfg = d->cfg;
      prv_pop(d);
      if (cfg.on_complete) {
        cfg.on_complete(false, NULL, 0, cfg.ctx);
      }
    }
  } else {
    prv_redraw(d);
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     prv_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN,   prv_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_handler);
  window_single_click_subscribe(BUTTON_ID_BACK,   prv_back_handler);
}

static void prv_window_unload(Window *window) {
  PinEntryWindowData *d = window_get_user_data(window);
  i18n_free_all(d);
  app_free(d);
}

void security_pin_entry_push(const SecurityPinEntryConfig *config) {
  PinEntryWindowData *d = app_malloc_check(sizeof(*d));
  *d = (PinEntryWindowData){
    .cfg = *config,
    .phase = Phase_Enter,
  };

  pin_entry_init(&d->entry, PIN_LOCK_MIN_LEN);

  window_init(&d->window, WINDOW_NAME("Security PIN Entry"));
  window_set_user_data(&d->window, d);
  window_set_background_color(&d->window, GColorWhite);
  window_set_click_config_provider_with_context(&d->window, prv_click_config_provider, d);
  window_set_window_handlers(&d->window, &(WindowHandlers){
    .unload = prv_window_unload,
  });
  layer_set_update_proc(window_get_root_layer(&d->window), prv_update_proc);

  app_window_stack_push(&d->window, true /* animated */);
}
