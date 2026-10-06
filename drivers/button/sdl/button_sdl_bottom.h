/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

enum {
  BUTTON_SDL_BACK,
  BUTTON_SDL_UP,
  BUTTON_SDL_SELECT,
  BUTTON_SDL_DOWN,
};

//! A button was pressed or released. Called by the bottom, on the main thread.
void button_sdl_changed(int button, bool pressed);
