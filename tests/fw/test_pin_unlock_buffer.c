/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "clar.h"
#include "popups/pin_lock/unlock_window.h"

#include "stubs_logging.h"
#include "stubs_passert.h"

// Stubs for UI and service symbols referenced by unlock_window.c but not
// exercised by these pure buffer tests.

#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/window_stack.h"
#include "applib/ui/layer.h"
#include "applib/ui/vibes.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/fonts/fonts.h"
#include "kernel/ui/modals/modal_manager.h"

void window_init(Window *w, const char *name) {}
void window_set_background_color(Window *w, GColor c) {}
void window_set_click_config_provider(Window *w, ClickConfigProvider p) {}
void window_set_overrides_back_button(Window *w, bool v) {}
Layer *window_get_root_layer(const Window *w) { return NULL; }
void layer_set_update_proc(Layer *l, LayerUpdateProc p) {}
void layer_mark_dirty(Layer *l) {}
bool window_stack_remove(Window *w, bool animated) { return false; }
void modal_window_push(Window *w, ModalPriority p, bool animated) {}
void window_single_click_subscribe(ButtonId id, ClickHandler h) {}
void graphics_context_set_fill_color(GContext *ctx, GColor c) {}
void graphics_context_set_stroke_color(GContext *ctx, GColor c) {}
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t r) {}
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t r) {}
void graphics_fill_rect(GContext *ctx, const GRect *r) {}
void graphics_draw_text(GContext *ctx, const char *text, GFont font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *attrs) {}
GFont fonts_get_system_font(const char *key) { return NULL; }
void vibes_double_pulse(void) {}
bool pin_lock_storage_verify_pin(const uint8_t *digits, uint8_t len) { return false; }
void pin_lock_mark_unlocked(void) {}
uint8_t pin_lock_get_pin_len(void) { return 4; }

void test_pin_unlock_buffer__initialize(void) {}
void test_pin_unlock_buffer__cleanup(void) {}

void test_pin_unlock_buffer__up_down_wrap(void) {
  PinEntry e; pin_entry_init(&e, 4);
  cl_assert_equal_i(0, e.digits[0]);
  pin_entry_down(&e);
  cl_assert_equal_i(9, e.digits[0]);
  pin_entry_up(&e);
  cl_assert_equal_i(0, e.digits[0]);
}

void test_pin_unlock_buffer__select_advances_and_signals_complete(void) {
  PinEntry e; pin_entry_init(&e, 4);
  cl_assert_equal_b(false, pin_entry_select(&e));
  cl_assert_equal_b(false, pin_entry_select(&e));
  cl_assert_equal_b(false, pin_entry_select(&e));
  cl_assert_equal_b(true,  pin_entry_select(&e));
}

void test_pin_unlock_buffer__back_signals_cancel_at_first(void) {
  PinEntry e; pin_entry_init(&e, 4);
  pin_entry_select(&e);
  cl_assert_equal_b(false, pin_entry_back(&e));
  cl_assert_equal_b(true,  pin_entry_back(&e));
}
