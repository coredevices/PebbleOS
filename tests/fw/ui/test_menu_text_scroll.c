/* SPDX-FileCopyrightText: 2026 Scott Anderson */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/ui/menu_text_scroll.h>
#include <clar.h>
#include <fake_app_timer.h>
#include <fake_pebble_tasks.h>
#include <fake_rtc.h>
#include <stubs_logging.h>

// Stubs
////////////////////////////////////

static int s_mark_dirty_count;

void layer_mark_dirty(Layer *layer) {
  s_mark_dirty_count++;
}

// Helpers
////////////////////////////////////

static Layer s_content_layer;
static Layer s_row_a;
static Layer s_row_b;

static void prv_set_ms(uint32_t ms) {
  fake_rtc_set_ticks(((RtcTicks)ms * RTC_TICKS_HZ + 999) / 1000);
}

static AppTimer *prv_timer(void) {
  return s_fake_app_timer_head ? (AppTimer *)(uintptr_t)s_fake_app_timer_head->timer_id : NULL;
}

// Tests
////////////////////////////////////

void test_menu_text_scroll__initialize(void) {
  fake_app_timer_init();
  s_mark_dirty_count = 0;
  prv_set_ms(0);
  s_content_layer = (Layer){};
  s_row_a = (Layer){.parent = &s_content_layer, .frame = GRect(0, 0, 200, 40)};
  s_row_b = (Layer){.parent = &s_content_layer, .frame = GRect(0, 40, 200, 40)};
}

void test_menu_text_scroll__cleanup(void) {
  menu_text_scroll_layer_deinit(&s_content_layer);
  fake_app_timer_deinit();
}

// 60 px of overflow scrolls for 1 s between 800 ms pauses, then starts over
void test_menu_text_scroll__pauses_scrolls_and_loops(void) {
  const int16_t overflow = 60;
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_a, overflow), 0);
  prv_set_ms(799);
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_a, overflow), 0);
  prv_set_ms(1300);
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_a, overflow), 30);
  prv_set_ms(1800);
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_a, overflow), overflow);
  prv_set_ms(2590);
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_a, overflow), overflow);
  prv_set_ms(2610);
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_a, overflow), 0);
}

void test_menu_text_scroll__new_row_starts_over(void) {
  prv_set_ms(1300);
  menu_text_scroll_get_offset(&s_row_a, 60);
  prv_set_ms(2000);
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_b, 60), 0);
  // Same row with different text also starts over
  prv_set_ms(3000);
  cl_assert_equal_i(menu_text_scroll_get_offset(&s_row_b, 90), 0);
}

void test_menu_text_scroll__redraws_until_drawing_stops(void) {
  menu_text_scroll_get_offset(&s_row_a, 60);
  cl_assert(prv_timer());

  // Still drawing: the timer marks the menu dirty and reschedules
  prv_set_ms(33);
  cl_assert(app_timer_trigger(prv_timer()));
  cl_assert_equal_i(s_mark_dirty_count, 1);
  cl_assert(prv_timer());

  // The row stopped drawing (e.g. scrolled away or deselected): the timer stops
  prv_set_ms(1000);
  cl_assert(app_timer_trigger(prv_timer()));
  cl_assert_equal_i(s_mark_dirty_count, 1);
  cl_assert(!prv_timer());
}

void test_menu_text_scroll__deinit_cancels_timer(void) {
  menu_text_scroll_get_offset(&s_row_a, 60);
  cl_assert(prv_timer());

  // Another menu's teardown leaves it alone
  Layer other = {};
  menu_text_scroll_layer_deinit(&other);
  cl_assert(prv_timer());

  menu_text_scroll_layer_deinit(&s_content_layer);
  cl_assert(!prv_timer());
}
