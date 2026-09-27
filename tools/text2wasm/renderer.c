/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdlib.h>
#include <string.h>
#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/text.h"
#include "applib/graphics/text_resources.h"
#include "stubs_app_state.h"
#include "stubs_applib_resource.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_logging.h"
#include "stubs_pebble_tasks.h"
#include "stubs_heap.h"
#include "stubs_pbl_malloc.h"
#include "stubs_memory_layout.h"

static const uint8_t *font_bytes;
static const uint8_t *extension_bytes;
static size_t extension_size;
static size_t font_size;
static FrameBuffer framebuffer;
static GContext context;
static uint8_t pixels[144 * 168 * 4];
size_t sys_resource_load_range(ResAppNum app, uint32_t id, uint32_t offset, uint8_t *buffer,
                               size_t size) {
  const uint8_t *bytes = id == 2 ? extension_bytes : font_bytes;
  size_t length = id == 2 ? extension_size : font_size;
  if (!bytes || offset > length || size > length - offset)
    return 0;
  memcpy(buffer, bytes + offset, size);
  return size;
}
bool sys_resource_is_valid(ResAppNum app, uint32_t id) {
  return id == 1 || (id == 2 && extension_size);
}
uint32_t sys_resource_get_and_cache(ResAppNum app, uint32_t id) {
  return id;
}
ResourceCallbackHandle resource_watch(ResAppNum app, uint32_t id, ResourceChangedCallback cb,
                                      void *data) {
  return NULL;
}
FontInfo *fonts_get_system_emoji_font_for_size(unsigned int size) {
  return NULL;
}
void passert_failed(const char *file, int line, const char *message, ...) {
  abort();
}
void passert_failed_no_message(const char *file, int line) {
  abort();
}
void passert_failed_no_message_with_lr(const char *file, int line, uint32_t lr) {
  abort();
}
uint8_t *render(const char *text, const uint8_t *font, size_t length, const uint8_t *extension,
                size_t extension_length) {
  font_bytes = font;
  font_size = length;
  extension_bytes = extension;
  extension_size = extension_length;
  FontInfo info = {0};
  if (!text_resources_init_font(0, 1, extension_length ? 2 : 0, &info))
    return NULL;
  framebuffer_init(&framebuffer, &(GSize){144, 168});
  graphics_context_init(&context, &framebuffer, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &context;
  framebuffer_clear(&framebuffer);
  graphics_context_set_text_color(&context, GColorBlack);
  graphics_draw_text(&context, text, &info, GRect(4, 4, 136, 160),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  for (int i = 0; i < 144 * 168; i++) {
    uint8_t c = framebuffer.buffer[i];
    pixels[4 * i] = ((c >> 4) & 3) * 85;
    pixels[4 * i + 1] = ((c >> 2) & 3) * 85;
    pixels[4 * i + 2] = (c & 3) * 85;
    pixels[4 * i + 3] = 255;
  }
  return pixels;
}

uint8_t fonts_get_font_height(GFont font) {
  return ((FontInfo *)font)->max_height;
}
void sys_font_reload_font(FontInfo *font) {
  abort();
}
GFont sys_font_get_system_font(const char *key) {
  return NULL;
}
void util_assertion_failed(const char *file, int line) {
  abort();
}
