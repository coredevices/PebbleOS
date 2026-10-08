/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "display_sdl_bottom.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <posix_host.h>
#include <pthread.h>
#include <sdl_bottom.h>

// Frames come from the firmware's display thread; the window is drawn on the
// main thread.

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t *s_fb;
static int s_width;
static int s_height;
static bool s_dirty;

static int s_scale = 1;
static const char *s_screenshot_path;

static SDL_Window *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture *s_texture;

static uint32_t prv_argb2222_to_argb8888(uint8_t c) {
  uint32_t r = ((c >> 4) & 0x3) * 0x55;
  uint32_t g = ((c >> 2) & 0x3) * 0x55;
  uint32_t b = (c & 0x3) * 0x55;
  return 0xff000000u | (r << 16) | (g << 8) | b;
}

void display_sdl_bottom_update(const uint8_t *fb, int width, int height) {
  pthread_mutex_lock(&s_lock);
  if (s_fb == NULL) {
    s_fb = malloc(sizeof(uint32_t) * width * height);
    s_width = width;
    s_height = height;
  }
  for (int i = 0; i < width * height; i++) {
    s_fb[i] = prv_argb2222_to_argb8888(fb[i]);
  }
  s_dirty = true;
  pthread_mutex_unlock(&s_lock);
  posix_host_wake();
}

static void prv_poll(void) {
  pthread_mutex_lock(&s_lock);
  if (!s_dirty) {
    pthread_mutex_unlock(&s_lock);
    return;
  }
  if (s_window == NULL) {
    s_window = SDL_CreateWindow("PebbleOS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                s_width * s_scale, s_height * s_scale, 0);
    SDL_RaiseWindow(s_window);
    s_renderer = SDL_CreateRenderer(s_window, -1, 0);
    // Input events then come in display coordinates, whatever the scale.
    SDL_RenderSetLogicalSize(s_renderer, s_width, s_height);
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  s_width, s_height);
  }
  SDL_UpdateTexture(s_texture, NULL, s_fb, s_width * (int)sizeof(uint32_t));
  s_dirty = false;
  pthread_mutex_unlock(&s_lock);

  SDL_RenderClear(s_renderer);
  SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
  SDL_RenderPresent(s_renderer);
}

static void prv_exit(void) {
  if (s_screenshot_path == NULL) {
    return;
  }
  pthread_mutex_lock(&s_lock);
  if (s_fb != NULL) {
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

static void prv_set_scale(const char *value) {
  s_scale = atoi(value);
}

static void prv_set_screenshot(const char *value) {
  s_screenshot_path = value;
}

POSIX_HOST_OPTION(.flag = 's', .arg = "scale", .help = "window scale factor", .set = prv_set_scale)
POSIX_HOST_OPTION(.flag = 'S', .arg = "out.bmp", .help = "save the last frame here when quitting",
                  .set = prv_set_screenshot)
