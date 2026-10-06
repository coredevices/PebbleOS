/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include "posix_host.h"
#include "sdl_bottom.h"

#define MAX_HANDLERS 8

static sdl_event_handler_t s_handlers[MAX_HANDLERS];
static size_t s_num_handlers;

void sdl_event_handler_register(sdl_event_handler_t handler) {
  if (s_num_handlers < MAX_HANDLERS) {
    s_handlers[s_num_handlers++] = handler;
  }
}

static void prv_init(void) {
  // Started from a terminal, the app is not activated on its own.
  SDL_SetHint(SDL_HINT_FORCE_RAISEWINDOW, "1");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    exit(1);
  }
}

static void prv_poll(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q &&
                                   (event.key.keysym.mod & KMOD_CTRL))) {
      posix_host_quit();
    }
    for (size_t i = 0; i < s_num_handlers; i++) {
      s_handlers[i](&event);
    }
  }
}

static void prv_exit(void) {
  SDL_Quit();
}

POSIX_HOST_HOOK(.init = prv_init, .poll = prv_poll, .exit = prv_exit)
