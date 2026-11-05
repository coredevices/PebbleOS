/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "display_sdl_bottom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <posix_host.h>
#include <pthread.h>
#include <sdl_bottom.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Frames come from the firmware's display thread; the window is drawn on the
// main thread.

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t *s_raw;
static uint32_t *s_fb;
static int s_width;
static int s_height;
static bool s_dirty;
static bool s_sunlight;

static int s_scale = 1;
static const char *s_screenshot_path;

static SDL_Window *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture *s_texture;

// The panel's 64 colors as seen on a watch in sunlight, RGB888 indexed by
// the pixel's RGB222 (the developer guides' color picker "Sunlight" palette).
static const uint32_t s_sunlight_lut[64] = {
  0x000000, 0x001e41, 0x004387, 0x0068ca, 0x2b4a2c, 0x27514f, 0x16638d, 0x007dce,
  0x5e9860, 0x5c9b72, 0x57a5a2, 0x4cb4db, 0x8ee391, 0x8ee69e, 0x8aebc0, 0x84f5f1,
  0x4a161b, 0x482748, 0x40488a, 0x2f6bcc, 0x564e36, 0x545454, 0x4f6790, 0x4180d0,
  0x759a64, 0x759d76, 0x71a6a4, 0x69b5dd, 0x9ee594, 0x9de7a0, 0x9becc2, 0x95f6f2,
  0x99353f, 0x983e5a, 0x955694, 0x8f74d2, 0x9d5b4d, 0x9d6064, 0x9a7099, 0x9587d5,
  0xafa072, 0xaea382, 0xababab, 0xa7bae2, 0xc9e89d, 0xc9eaa7, 0xc7f0c8, 0xc3f9f7,
  0xe35462, 0xe25874, 0xe16aa3, 0xde83dc, 0xe66e6b, 0xe6727c, 0xe37fa7, 0xe194df,
  0xf1aa86, 0xf1ad93, 0xefb5b8, 0xecc3eb, 0xffeeab, 0xfff1b5, 0xfff6d3, 0xffffff,
};

static uint32_t prv_argb2222_to_argb8888(uint8_t c) {
  if (s_sunlight) {
    return 0xff000000u | s_sunlight_lut[c & 0x3f];
  }
  uint32_t r = ((c >> 4) & 0x3) * 0x55;
  uint32_t g = ((c >> 2) & 0x3) * 0x55;
  uint32_t b = (c & 0x3) * 0x55;
  return 0xff000000u | (r << 16) | (g << 8) | b;
}

// With s_lock held.
static void prv_convert(void) {
  for (int i = 0; i < s_width * s_height; i++) {
    s_fb[i] = prv_argb2222_to_argb8888(s_raw[i]);
  }
  s_dirty = true;
}

void display_sdl_bottom_update(const uint8_t *fb, int width, int height) {
  pthread_mutex_lock(&s_lock);
  if (s_fb == nullptr) {
    s_raw = malloc(width * height);
    s_fb = malloc(sizeof(uint32_t) * width * height);
    s_width = width;
    s_height = height;
  }
  memcpy(s_raw, fb, width * height);
  prv_convert();
  pthread_mutex_unlock(&s_lock);
  posix_host_wake();
}

// Main thread.
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void display_sdl_bottom_set_sunlight(bool sunlight) {
  pthread_mutex_lock(&s_lock);
  s_sunlight = sunlight;
  if (s_fb != nullptr) {
    prv_convert();
  }
  pthread_mutex_unlock(&s_lock);
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool display_sdl_bottom_sunlight(void) {
  return s_sunlight;
}

static void prv_poll(void) {
  pthread_mutex_lock(&s_lock);
  if (!s_dirty) {
    pthread_mutex_unlock(&s_lock);
    return;
  }
  if (s_window == nullptr) {
    s_window = SDL_CreateWindow("PebbleOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                s_width * s_scale, s_height * s_scale, 0);
    SDL_RaiseWindow(s_window);
    s_renderer = SDL_CreateRenderer(s_window, -1, 0);
    // Input events then come in display coordinates, whatever the scale.
    SDL_RenderSetLogicalSize(s_renderer, s_width, s_height);
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  s_width, s_height);
  }
  SDL_UpdateTexture(s_texture, nullptr, s_fb, s_width * (int)sizeof(uint32_t));
  s_dirty = false;
  pthread_mutex_unlock(&s_lock);

  SDL_RenderClear(s_renderer);
  SDL_RenderCopy(s_renderer, s_texture, nullptr, nullptr);
  SDL_RenderPresent(s_renderer);
}

static void prv_exit(void) {
  if (s_screenshot_path == nullptr) {
    return;
  }
  pthread_mutex_lock(&s_lock);
  if (s_fb != nullptr) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        s_fb, s_width, s_height, 32, s_width * (int)sizeof(uint32_t), SDL_PIXELFORMAT_ARGB8888);
    if (SDL_SaveBMP(surface, s_screenshot_path) != 0) {
      fprintf(stderr, "cannot save %s: %s\n", s_screenshot_path, SDL_GetError());
    }
    SDL_FreeSurface(surface);
  }
  pthread_mutex_unlock(&s_lock);
}

POSIX_HOST_HOOK(.poll = prv_poll, .exit = prv_exit)

static void prv_handle_event(const SDL_Event *event) {
  if (event->type == SDL_KEYDOWN && !event->key.repeat && event->key.keysym.sym == SDLK_l &&
      !(event->key.keysym.mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT))) {
    display_sdl_bottom_set_sunlight(!s_sunlight);
  }
}

SDL_EVENT_HANDLER(prv_handle_event)

static void prv_set_scale(const char *value) {
  s_scale = atoi(value);
}

static void prv_set_screenshot(const char *value) {
  s_screenshot_path = value;
}

static void prv_set_sunlight(const char *value) {
  s_sunlight = true;
}

POSIX_HOST_OPTION(.flag = 's', .arg = "scale", .help = "window scale factor", .set = prv_set_scale)
POSIX_HOST_OPTION(.flag = 'S', .arg = "out.bmp", .help = "save the last frame here when quitting",
                  .set = prv_set_screenshot)
POSIX_HOST_OPTION(.flag = 'l', .help = "show the colors as in sunlight (toggle with L)",
                  .set = prv_set_sunlight)
