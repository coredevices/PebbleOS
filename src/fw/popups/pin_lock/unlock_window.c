/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "unlock_window.h"

#include "applib/graphics/gcolor_definitions.h"
#include "applib/graphics/gcontext.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/window_stack.h"
#include "board/display.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/i18n/i18n.h"
#include "pin_flap.h"
#include "services/pin_lock/pin_lock.h"
#include "shell/prefs.h"
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

static Window   s_window;
static PinEntry s_entry;
static PinFlap  s_flap;

static void prv_update_proc(Layer *layer, GContext *ctx) {
  // A custom root-layer update proc replaces the window's default background
  // fill, so paint it ourselves to fully cover the app/watchface below.
  const GRect bg = GRect(0, 0, DISP_COLS, DISP_ROWS);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &bg);

  // Refresh prefs each frame (can change while the window is open).
  s_flap.config.mask_confirmed = pin_lock_should_mask_digits();
  s_flap.config.haptic = pin_lock_should_haptic();
  pin_flap_draw(&s_flap, ctx, GRect(0, 0, DISP_COLS, DISP_ROWS));
}

static void prv_pop(void) {
  pin_flap_reset(&s_flap);
  window_stack_remove(&s_window, true /* animated */);
}

static void prv_on_unlocked(void *ctx) {
  pin_lock_mark_unlocked();
  prv_pop();
}

static void prv_up_handler(ClickRecognizerRef recognizer, void *context) {
  const uint8_t old = s_entry.digits[s_entry.pos];
  pin_entry_up(&s_entry);
  pin_flap_animate_step(&s_flap, window_get_root_layer(&s_window), old, +1);
}

static void prv_down_handler(ClickRecognizerRef recognizer, void *context) {
  const uint8_t old = s_entry.digits[s_entry.pos];
  pin_entry_down(&s_entry);
  pin_flap_animate_step(&s_flap, window_get_root_layer(&s_window), old, -1);
}

static void prv_select_handler(ClickRecognizerRef recognizer, void *context) {
  if (!pin_entry_select(&s_entry)) {
    layer_mark_dirty(window_get_root_layer(&s_window));
    return;
  }
  // All digits confirmed — verify the PIN.
  if (pin_lock_storage_verify_pin(s_entry.digits, s_entry.len)) {
    pin_flap_padlock_open(&s_flap, window_get_root_layer(&s_window),
                          prv_on_unlocked, NULL);
  } else {
    PBL_LOG_DBG("PIN unlock: incorrect PIN, resetting entry");
    vibes_double_pulse();
    pin_entry_init(&s_entry, s_entry.len);
    // Reset roll state only; padlock_shake manages the lock animation itself.
    if (s_flap.anim) {
      animation_unschedule(s_flap.anim);
      animation_destroy(s_flap.anim);
      s_flap.anim = NULL;
    }
    s_flap.animating = false;
    s_flap.progress = 0;
    pin_flap_padlock_shake(&s_flap, window_get_root_layer(&s_window));
    layer_mark_dirty(window_get_root_layer(&s_window));
  }
}

static void prv_back_handler(ClickRecognizerRef recognizer, void *context) {
  if (pin_entry_back(&s_entry)) {
    // At first position: cancel (stay locked) and dismiss.
    prv_pop();
  } else {
    layer_mark_dirty(window_get_root_layer(&s_window));
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

  PinFlapConfig cfg = {
    .entry          = &s_entry,
    .title          = i18n_noop("Enter PIN"),
    .mask_confirmed = pin_lock_should_mask_digits(),
    .accent         = shell_prefs_get_theme_highlight_color(),
  };
  pin_flap_init(&s_flap, &cfg);

  window_init(&s_window, WINDOW_NAME("PIN Unlock"));
  window_set_background_color(&s_window, GColorWhite);
  // Prevent the default back-button pop from bypassing the lock.
  window_set_overrides_back_button(&s_window, true);
  window_set_click_config_provider(&s_window, prv_click_config_provider);
  layer_set_update_proc(window_get_root_layer(&s_window), prv_update_proc);

  modal_window_push(&s_window, ModalPriorityCritical, true /* animated */);
}
