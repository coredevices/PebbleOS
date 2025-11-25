/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sdl_app.h"
#include "sdl_graphics.h"

#include <stdio.h>

extern int app_main(void);

int main(int argc, char **argv) {
  if (!sdl_app_init()) {
    return -1;
  }

  app_main();
  sdl_app_deinit();

  return 0;
}

#include <SDL.h>

bool sdl_app_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("Error: Failed to init SDL\n");
    return false;
  }

  if (!sdl_graphics_init()) {
    printf("Error: Failed to init graphics\n");
    return false;
  }

  return true;
}

void sdl_app_deinit(void) {
  SDL_Quit();
}

void sdl_app_event_loop(void) {
  SDL_Event event;
  int keypress = 0;

  while (!keypress) {
    sdl_graphics_render();
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          keypress = 1;
          break;
        case SDL_KEYDOWN:
          keypress = 1;
          break;
      }
    }
  }
}
