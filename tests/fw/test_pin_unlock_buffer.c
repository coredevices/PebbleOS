/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "clar.h"
#include "popups/pin_lock/unlock_window.h"
#include "popups/pin_lock/pin_drum.h"

#include "stubs_logging.h"
#include "stubs_passert.h"

// Stubs for UI and service symbols referenced by unlock_window.c and
// pin_drum.c but not exercised by these pure buffer / helper tests.

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
void graphics_context_set_text_color(GContext *ctx, GColor c) {}
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t r) {}
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t r) {}
void graphics_fill_rect(GContext *ctx, const GRect *r) {}
void graphics_fill_round_rect(GContext *ctx, const GRect *r, uint16_t radius,
                              GCornerMask corner_mask) {}
void graphics_draw_round_rect(GContext *ctx, const GRect *r, uint16_t radius) {}
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1) {}
void graphics_draw_text(GContext *ctx, const char *text, GFont font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *attrs) {}
GDrawState graphics_context_get_drawing_state(GContext *ctx) { return (GDrawState){}; }
void graphics_context_set_drawing_state(GContext *ctx, GDrawState state) {}
GFont fonts_get_system_font(const char *key) { return NULL; }
uint8_t fonts_get_font_height(GFont font) { return 0; }
void vibes_double_pulse(void) {}
void vibes_enqueue_custom_pattern(VibePattern pattern) {}
bool pin_lock_storage_verify_pin(const uint8_t *digits, uint8_t len) { return false; }
void pin_lock_mark_unlocked(void) {}
uint8_t pin_lock_get_pin_len(void) { return 4; }
bool pin_lock_should_mask_digits(void) { return false; }
bool pin_lock_should_haptic(void) { return false; }
GColor shell_prefs_get_theme_highlight_color(void) { return (GColor){}; }
// i18n stubs
const char *i18n_get(const char *msgid, const void *owner) { return msgid; }
void i18n_free_all(const void *owner) {}
// Animation stubs — pin_drum_animate_step is not exercised by buffer tests.
Animation *animation_create(void) { return NULL; }
bool animation_destroy(Animation *a) { return true; }
bool animation_set_duration(Animation *a, uint32_t ms) { return true; }
bool animation_set_curve(Animation *a, AnimationCurve c) { return true; }
bool animation_set_implementation(Animation *a,
                                  const AnimationImplementation *impl) { return true; }
bool animation_set_handlers(Animation *a, AnimationHandlers h, void *ctx) { return true; }
bool animation_schedule(Animation *a) { return true; }
bool animation_unschedule(Animation *a) { return true; }
void *animation_get_context(Animation *a) { return NULL; }

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

void test_pin_unlock_buffer__roll_offset_endpoints(void) {
  cl_assert_equal_i(0,  pin_drum_roll_offset(0, 40));
  cl_assert_equal_i(40, pin_drum_roll_offset(65535, 40));
  cl_assert_equal_i(20, pin_drum_roll_offset(32768, 40));
}
