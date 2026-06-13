/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "unlock_window.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/gcolor_definitions.h"
#include "applib/graphics/gcontext.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_circle.h"
#include "applib/graphics/text.h"
#include "applib/ui/layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/window_stack.h"
#include "board/display.h"
#include "kernel/ui/modals/modal_manager.h"
#include "services/pin_lock/pin_lock.h"
#include "system/logging.h"

// ── entry-buffer ──────────────────────────────────────────────────────────────

void pin_entry_init(PinEntry *e, uint8_t len) {
  *e = (PinEntry){ .len = len, .pos = 0 };
}

void pin_entry_up(PinEntry *e) {
  e->digits[e->pos] = (e->digits[e->pos] + 1) % 10;
}

void pin_entry_down(PinEntry *e) {
  e->digits[e->pos] = (e->digits[e->pos] + 9) % 10;
}

bool pin_entry_select(PinEntry *e) {
  if (e->pos + 1 >= e->len) {
    return true;
  }
  e->pos++;
  return false;
}

bool pin_entry_back(PinEntry *e) {
  if (e->pos == 0) {
    return true;
  }
  e->pos--;
  return false;
}

// ── modal window ──────────────────────────────────────────────────────────────

// Dot layout constants — pixels, centred on the display.
#define DOT_RADIUS        7
#define DOT_SPACING      20
#define DOT_ROW_Y       (DISP_ROWS / 2)
// Box around the active digit numeral (must comfortably contain a 28pt glyph).
#define DIGIT_HALF      (DOT_RADIUS * 2)

static Window   s_window;
static PinEntry s_entry;

static void prv_redraw(void) {
  layer_mark_dirty(window_get_root_layer(&s_window));
}

// Draw len positions centred on the display:
//   index < pos  → filled dot (confirmed digit)
//   index == pos → decimal numeral for the current digit value
//   index > pos  → empty ring (not yet entered)
static void prv_update_proc(Layer *layer, GContext *ctx) {
  // A custom root-layer update proc replaces the window's default background
  // fill, so paint it ourselves to fully cover the app/watchface below.
  const GRect bg = GRect(0, 0, DISP_COLS, DISP_ROWS);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &bg);

  const uint8_t n = s_entry.len;
  // Total row spans (n-1) gaps plus two half-radii on the outer edges.
  const int16_t total_w = (n - 1) * DOT_SPACING;
  const int16_t start_x = (DISP_COLS - total_w) / 2;

  for (uint8_t i = 0; i < n; i++) {
    const int16_t cx = start_x + i * DOT_SPACING;
    const GPoint centre = GPoint(cx, DOT_ROW_Y);

    if (i < s_entry.pos) {
      // Confirmed — filled dot.
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_circle(ctx, centre, DOT_RADIUS);
    } else if (i == s_entry.pos) {
      // Active — show current digit as a numeral.
      char buf[2] = { (char)('0' + s_entry.digits[i]), '\0' };
      GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
      GRect box = GRect(cx - DIGIT_HALF, DOT_ROW_Y - DIGIT_HALF,
                        DIGIT_HALF * 2, DIGIT_HALF * 2);
      // Clear the background so the digit is readable.
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, &box);
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_draw_text(ctx, buf, font, box,
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    } else {
      // Not yet reached — empty ring.
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_circle(ctx, centre, DOT_RADIUS);
    }
  }
}

static void prv_pop(void) {
  window_stack_remove(&s_window, true /* animated */);
}

static void prv_up_handler(ClickRecognizerRef recognizer, void *context) {
  pin_entry_up(&s_entry);
  prv_redraw();
}

static void prv_down_handler(ClickRecognizerRef recognizer, void *context) {
  pin_entry_down(&s_entry);
  prv_redraw();
}

static void prv_select_handler(ClickRecognizerRef recognizer, void *context) {
  if (!pin_entry_select(&s_entry)) {
    prv_redraw();
    return;
  }
  // All digits confirmed — verify the PIN.
  if (pin_lock_storage_verify_pin(s_entry.digits, s_entry.len)) {
    pin_lock_mark_unlocked();
    prv_pop();
  } else {
    PBL_LOG_DBG("PIN unlock: incorrect PIN, resetting entry");
    vibes_double_pulse();
    pin_entry_init(&s_entry, s_entry.len);
    prv_redraw();
  }
}

static void prv_back_handler(ClickRecognizerRef recognizer, void *context) {
  if (pin_entry_back(&s_entry)) {
    // At first position: cancel (stay locked) and dismiss.
    prv_pop();
  } else {
    prv_redraw();
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     prv_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN,   prv_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_handler);
  // Override BACK so a short press steps back rather than popping generically.
  window_single_click_subscribe(BUTTON_ID_BACK,   prv_back_handler);
}

void pin_unlock_window_push(void) {
  pin_entry_init(&s_entry, pin_lock_get_pin_len());

  window_init(&s_window, WINDOW_NAME("PIN Unlock"));
  window_set_background_color(&s_window, GColorWhite);
  // Prevent the default back-button pop from bypassing the lock.
  window_set_overrides_back_button(&s_window, true);
  window_set_click_config_provider(&s_window, prv_click_config_provider);
  layer_set_update_proc(window_get_root_layer(&s_window), prv_update_proc);

  modal_window_push(&s_window, ModalPriorityCritical, true /* animated */);
}
