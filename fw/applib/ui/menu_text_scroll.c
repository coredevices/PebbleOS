/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "menu_text_scroll.h"

#include "applib/app_timer.h"
#include "kernel/pebble_tasks.h"
#include "pbl/drivers/rtc.h"
#include "pbl/util/math.h"
#include "syscall/syscall.h"

#define FRAME_INTERVAL_MS 33
#define PIXELS_PER_SECOND 60
#define PAUSE_MS          800
//! Stop redrawing once the highlighted cell has not drawn for this long. Generous, since a slow
//! render must not end the scroll early.
#define STALL_MS 300

typedef struct {
  //! Content layer of the menu being scrolled, and the row in it
  const Layer *layer;
  int16_t row_y;
  int16_t overflow;
  AppTimer *timer;
  uint32_t start_ms;
  uint32_t last_draw_ms;
} MenuTextScroll;

// Only the app and kernel tasks draw menus
static MenuTextScroll s_scrolls[2];

static MenuTextScroll *prv_get_scroll(void) {
  return &s_scrolls[(pebble_task_get_current() == PebbleTask_App) ? 0 : 1];
}

static uint32_t prv_now_ms(void) {
  return (uint32_t)((sys_get_ticks() * 1000 + RTC_TICKS_HZ / 2) / RTC_TICKS_HZ);
}

static void prv_timer_cb(void *data) {
  MenuTextScroll *scroll = data;
  scroll->timer = NULL;
  // Keep going only while the highlighted cell is still drawing overflowing text
  if (prv_now_ms() - scroll->last_draw_ms > STALL_MS) {
    return;
  }
  scroll->timer = app_timer_register(FRAME_INTERVAL_MS, prv_timer_cb, scroll);
  layer_mark_dirty((Layer *)scroll->layer);
}

//! Wait, scroll to the end, wait, then start over
int16_t menu_text_scroll_get_offset(const Layer *cell_layer, int16_t overflow) {
  MenuTextScroll *scroll = prv_get_scroll();
  const uint32_t now = prv_now_ms();

  // Another cell or other text starts from the beginning
  if (scroll->layer != cell_layer->parent || scroll->row_y != cell_layer->frame.origin.y ||
      scroll->overflow != overflow) {
    scroll->layer = cell_layer->parent;
    scroll->row_y = cell_layer->frame.origin.y;
    scroll->overflow = overflow;
    scroll->start_ms = now;
  }
  scroll->last_draw_ms = now;
  if (!scroll->timer) {
    scroll->timer = app_timer_register(FRAME_INTERVAL_MS, prv_timer_cb, scroll);
  }

  const uint32_t scroll_ms = ((uint32_t)overflow * 1000) / PIXELS_PER_SECOND;
  const uint32_t t = (now - scroll->start_ms) % (PAUSE_MS + scroll_ms + PAUSE_MS);
  if (t < PAUSE_MS) {
    return 0;
  }
  return (int16_t)MIN((int32_t)(((t - PAUSE_MS) * PIXELS_PER_SECOND) / 1000), (int32_t)overflow);
}

void menu_text_scroll_layer_deinit(const Layer *content_layer) {
  MenuTextScroll *scroll = prv_get_scroll();
  if (scroll->layer != content_layer) {
    return;
  }
  if (scroll->timer) {
    app_timer_cancel(scroll->timer);
  }
  *scroll = (MenuTextScroll){};
}
