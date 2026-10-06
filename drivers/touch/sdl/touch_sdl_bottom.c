/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

#include "sdl_bottom.h"
#include "touch_sdl_bottom.h"

static bool s_pressed;

static void prv_handle_event(const SDL_Event *event) {
  switch (event->type) {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      s_pressed = (event->type == SDL_MOUSEBUTTONDOWN);
      touch_sdl_changed(s_pressed, event->button.x, event->button.y);
      break;
    case SDL_MOUSEMOTION:
      if (s_pressed) {
        touch_sdl_changed(true, event->motion.x, event->motion.y);
      }
      break;
    default:
      break;
  }
}

SDL_EVENT_HANDLER(prv_handle_event)
