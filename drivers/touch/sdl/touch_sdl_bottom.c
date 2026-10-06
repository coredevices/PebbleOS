/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

#include "posix_host.h"
#include "sdl_bottom.h"
#include "touch_sdl_bottom.h"

// The mouse is a finger. Scrolling, with a wheel or two fingers on a
// trackpad, drags a finger from the pointer, and lifts it once the scrolling
// pauses.

#define SCROLL_PX_PER_UNIT 16
#define SCROLL_RELEASE_US  100000

static int s_width;
static int s_height;

static bool s_pressed;
static int s_mouse_x;
static int s_mouse_y;

static bool s_scrolling;
static float s_scroll_x;
static float s_scroll_y;
static uint64_t s_scroll_last_us;

void touch_sdl_bottom_init(int width, int height) {
  s_width = width;
  s_height = height;
}

static void prv_scroll_release(void) {
  s_scrolling = false;
  touch_sdl_changed(false, (int)s_scroll_x, (int)s_scroll_y);
}

static void prv_scroll(float dx, float dy) {
  if (s_pressed) {
    return;
  }
  if (!s_scrolling) {
    s_scrolling = true;
    s_scroll_x = s_mouse_x;
    s_scroll_y = s_mouse_y;
    touch_sdl_changed(true, s_mouse_x, s_mouse_y);
  }

  // The finger follows the content, as with natural scrolling; SDL reports
  // the deltas flipped when the host scrolls the other way.
  float x = s_scroll_x - dx * SCROLL_PX_PER_UNIT;
  float y = s_scroll_y + dy * SCROLL_PX_PER_UNIT;

  // Off the display, lift the finger and put it back where the drag started.
  if (x < 0 || x >= s_width || y < 0 || y >= s_height) {
    prv_scroll_release();
    s_scrolling = true;
    x = s_mouse_x - dx * SCROLL_PX_PER_UNIT;
    y = s_mouse_y + dy * SCROLL_PX_PER_UNIT;
    touch_sdl_changed(true, s_mouse_x, s_mouse_y);
  }

  s_scroll_x = x;
  s_scroll_y = y;
  s_scroll_last_us = posix_host_monotonic_us();
  touch_sdl_changed(true, (int)x, (int)y);
}

static void prv_handle_event(const SDL_Event *event) {
  switch (event->type) {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      if (s_scrolling) {
        prv_scroll_release();
      }
      s_pressed = (event->type == SDL_MOUSEBUTTONDOWN);
      touch_sdl_changed(s_pressed, event->button.x, event->button.y);
      break;
    case SDL_MOUSEMOTION:
      s_mouse_x = event->motion.x;
      s_mouse_y = event->motion.y;
      if (s_pressed) {
        touch_sdl_changed(true, event->motion.x, event->motion.y);
      }
      break;
    case SDL_MOUSEWHEEL:
      prv_scroll(event->wheel.preciseX, event->wheel.preciseY);
      break;
    default:
      break;
  }
}

SDL_EVENT_HANDLER(prv_handle_event)

static void prv_poll(void) {
  if (s_scrolling && posix_host_monotonic_us() - s_scroll_last_us > SCROLL_RELEASE_US) {
    prv_scroll_release();
  }
}

POSIX_HOST_HOOK(.poll = prv_poll)
