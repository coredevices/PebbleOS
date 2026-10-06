/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "button_sdl_bottom.h"
#include "sdl_bottom.h"

static int prv_key_to_button(SDL_Keycode key) {
  switch (key) {
    case SDLK_LEFT:
    case SDLK_BACKSPACE:
    case SDLK_ESCAPE:
      return BUTTON_SDL_BACK;
    case SDLK_UP:
      return BUTTON_SDL_UP;
    case SDLK_RIGHT:
    case SDLK_RETURN:
    case SDLK_SPACE:
      return BUTTON_SDL_SELECT;
    case SDLK_DOWN:
      return BUTTON_SDL_DOWN;
    default:
      return -1;
  }
}

static void prv_handle_event(const SDL_Event *event) {
  if ((event->type != SDL_KEYDOWN && event->type != SDL_KEYUP) || event->key.repeat) {
    return;
  }
  const int button = prv_key_to_button(event->key.keysym.sym);
  if (button >= 0) {
    button_sdl_changed(button, event->type == SDL_KEYDOWN);
  }
}

SDL_EVENT_HANDLER(prv_handle_event)
