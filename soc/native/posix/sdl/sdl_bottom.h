/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <SDL.h>

// SDL for the bottom halves of drivers: initialized on the main thread, whose
// event loop hands each event to the registered handlers.

typedef void (*sdl_event_handler_t)(const SDL_Event *event);

//! Handlers get every event, on the main thread.
void sdl_event_handler_register(sdl_event_handler_t handler);

//! Registers an event handler at startup.
#define SDL_EVENT_HANDLER(_handler)                                            \
  __attribute__((constructor)) static void prv_sdl_register_##_handler(void) { \
    sdl_event_handler_register(_handler);                                      \
  }
