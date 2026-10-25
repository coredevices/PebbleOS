/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "call_indicator.h"

#include <stdio.h>
#include <string.h>

#include <pbl/drivers/rtc.h>
#include <pbl/services/evented_timer.h>
#include <pbl/services/light.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/graphics.h>
#include <applib/ui/window.h>
#include <applib/ui/window_stack.h>
#include <kernel/event_loop.h>
#include <kernel/ui/modals/modal_manager.h>
#include <popups/phone_ui.h>
#include <zbt/zbt.h>

#define BAR_H      30
#define REFRESH_MS 1000

#define COMBO_BUTTONS ((1 << BUTTON_ID_SELECT) | (1 << BUTTON_ID_DOWN))

static struct {
  Window window;
  bool initialized;
  bool shown;
  bool pressed;
  bool was_in_call;
  EventedTimerID timer;
  char text[40];
  //! Buttons held, and whether they made the combo (their events stay from the app).
  uint8_t buttons;
  bool combo;
  //! For call_indicator_open_call_screen().
  char dialed[ZBT_CLIP_MAX];
} s_ind;

static void prv_draw(Layer *layer, GContext *ctx) {
  const GRect bounds = layer->bounds;
  const GRect bar = GRect(0, 0, bounds.size.w, BAR_H);
  graphics_context_set_fill_color(ctx, GColorIslamicGreen);
  graphics_fill_rect(ctx, &bar);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_ind.text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, 2, bounds.size.w - 8, BAR_H - 2), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static bool prv_in_call(const struct zbt_status *st) {
  // Not Android's virtual call: the phone's audio for an app, which shows no call.
  return st->state == ZBT_STATE_READY && !st->call_virtual &&
         (st->call == ZBT_CALL_OUTGOING || st->call == ZBT_CALL_ALERTING ||
          st->call == ZBT_CALL_ACTIVE || st->call == ZBT_CALL_HELD);
}

// The call's screen, on KernelMain: the number dialed or the one the host has, with its name.
static void prv_open(void *data) {
  struct zbt_status st;
  zbt_get_status(&st);
  const char *number = s_ind.dialed[0] != '\0' ? s_ind.dialed : st.clip;
  char name[ZBT_NAME_MAX] = "";
  (void)zbt_book_find_name(number, name, sizeof(name));
  phone_ui_show_watch_call(number, name, st.call_since);
  s_ind.dialed[0] = '\0';
  call_indicator_update();
}

void call_indicator_open_call_screen(const char *number) {
  strncpy(s_ind.dialed, number, sizeof(s_ind.dialed) - 1);
  s_ind.dialed[sizeof(s_ind.dialed) - 1] = '\0';
  launcher_task_add_callback(prv_open, NULL);
}

static void prv_refresh(void *data) {
  call_indicator_update();
}

void call_indicator_update(void) {
  if (!s_ind.initialized) {
    window_init(&s_ind.window, WINDOW_NAME("Call indicator"));
    window_set_focusable(&s_ind.window, false);
    window_set_transparent(&s_ind.window, true);
    layer_set_update_proc(&s_ind.window.layer, prv_draw);
    s_ind.initialized = true;
  }
  struct zbt_status st;
  zbt_get_status(&st);
  const bool in_call = prv_in_call(&st);
  // The call's screen follows the call: answered, or gone.
  if (in_call || s_ind.was_in_call) {
    phone_ui_watch_call_changed(st.call_since, !in_call);
  }
  s_ind.was_in_call = in_call;

  const bool show = in_call && !phone_ui_is_visible();
  if (show) {
    if (st.call_since != 0) {
      const unsigned s = (unsigned)((uint32_t)rtc_get_time() - st.call_since);
      snprintf(s_ind.text, sizeof(s_ind.text), "In call %u:%02u  - tap", s / 60, s % 60);
    } else {
      snprintf(s_ind.text, sizeof(s_ind.text), "Calling...  - tap");
    }
    if (!s_ind.shown) {
      modal_window_push(&s_ind.window, ModalPriorityDiscreet, false /* animated */);
      s_ind.shown = true;
    }
    layer_mark_dirty(&s_ind.window.layer);
  } else if (s_ind.shown) {
    window_stack_remove(&s_ind.window, false /* animated */);
    s_ind.shown = false;
    s_ind.pressed = false;
  }
  if (in_call && s_ind.timer == EVENTED_TIMER_INVALID_ID) {
    s_ind.timer = evented_timer_register(REFRESH_MS, true /* repeating */, prv_refresh, NULL);
  } else if (!in_call && s_ind.timer != EVENTED_TIMER_INVALID_ID) {
    evented_timer_cancel(s_ind.timer);
    s_ind.timer = EVENTED_TIMER_INVALID_ID;
  }
}

// The bar and a margin under it, so a touch near it does not reach the app.
static GRect prv_area(void) {
  return GRect(0, 0, s_ind.window.layer.bounds.size.w, BAR_H + 8);
}

bool call_indicator_covers(int16_t x, int16_t y) {
  const GRect area = prv_area();
  const GPoint p = GPoint(x, y);
  return s_ind.shown && grect_contains_point(&area, &p);
}

bool call_indicator_handle_touch(const TouchEvent *event) {
  if (!s_ind.shown) {
    s_ind.pressed = false;
    return false;
  }
  const GRect area = prv_area();
  const GPoint p = GPoint(event->x, event->y);
  switch (event->type) {
    case TouchEvent_Touchdown:
      // Only on a lit screen: a stray touch on a dark one does nothing.
      s_ind.pressed = !event->non_navigational && light_is_on() && grect_contains_point(&area, &p);
      return s_ind.pressed;
    case TouchEvent_PositionUpdate:
      return s_ind.pressed;
    case TouchEvent_Liftoff:
      if (!s_ind.pressed) {
        return false;
      }
      s_ind.pressed = false;
      if (grect_contains_point(&area, &p)) {
        prv_open(NULL);
      }
      return true;
  }
  return false;
}

bool call_indicator_handle_button(const PebbleEvent *event) {
  const uint8_t bit = 1 << event->button.button_id;
  if (event->type == PEBBLE_BUTTON_DOWN_EVENT) {
    s_ind.buttons |= bit;
    if ((s_ind.buttons & COMBO_BUTTONS) == COMBO_BUTTONS && !s_ind.combo) {
      struct zbt_status st;
      zbt_get_status(&st);
      if (prv_in_call(&st) && !phone_ui_is_visible()) {
        s_ind.combo = true;
        prv_open(NULL);
      }
    }
  } else if (event->type == PEBBLE_BUTTON_UP_EVENT) {
    s_ind.buttons &= ~bit;
  }
  const bool taken = s_ind.combo && (bit & COMBO_BUTTONS);
  if (s_ind.combo && (s_ind.buttons & COMBO_BUTTONS) == 0) {
    s_ind.combo = false;
  }
  return taken;
}
