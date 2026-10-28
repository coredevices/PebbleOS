/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

enum {
  INPUT_SDL_KEY_BACK,
  INPUT_SDL_KEY_UP,
  INPUT_SDL_KEY_SELECT,
  INPUT_SDL_KEY_DOWN,
};

//! A key was pressed or released. Called by the bottom, on the main thread.
void input_sdl_key_changed(int key, bool pressed);
