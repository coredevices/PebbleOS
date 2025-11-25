/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "util/heap.h"
#include "util/size.h"
#include "applib/app_logging.h"
#include "applib/fonts/fonts.h"
#include "applib/fonts/fonts_private.h"
#include "applib/graphics/text_resources.h"
#include "applib/rockyjs/api/rocky_api.h"
#include "resource/resource_ids.auto.h"
#include "font_resource_keys.auto.h"
#include "font_resource_table.auto.h"

#include "emscripten_app.h"
#include "emscripten_graphics.h"
#include "emscripten_resources.h"

#include <emscripten/emscripten.h>

#define NUM_SYSTEM_FONTS (ARRAY_LENGTH(s_font_resource_keys))

void *task_malloc(size_t bytes) {
  return malloc(bytes);
}

void *task_zalloc(size_t bytes) {
  void *ptr = malloc(bytes);
  if (ptr) {
    memset(ptr, 0, bytes);
  }
  return ptr;
}

void *task_zalloc_check(size_t bytes) {
  void *ptr = task_zalloc(bytes);
  if (!ptr) {
    wtf();
  }
  return ptr;
}

void *task_realloc(void *ptr, size_t bytes) {
  return realloc(ptr, bytes);
}

void task_free(void *ptr) {
  free(ptr);
}

void app_log(uint8_t log_level, const char* src_filename,
             int src_line_number, const char* fmt, ...) {
  printf("%s:%d", src_filename, src_line_number);
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}

GContext* app_state_get_graphics_context() {
  return emx_graphics_get_gcontext();
}

bool app_state_get_text_perimeter_debugging_enabled(void) {
  return false;
}

Heap *app_state_get_heap(void) {
  return NULL;
}

GBitmap* app_state_legacy2_get_2bit_framebuffer(void) {
  return NULL;
}

bool heap_is_allocated(Heap* const heap, void* ptr) {
  return false;
}

void passert_failed(const char* filename, int line_number, const char* message, ...) {
  APP_LOG(LOG_LEVEL_ERROR, "ASSERTION FAILED: %s:%d", filename, line_number);
  EM_ASM_INT_V({ abort(); });
  while (1) ;
}

void passert_failed_no_message(const char* filename, int line_number) {
  passert_failed(filename, line_number, NULL);
  while (1) ;
}

void passert_failed_hashed_no_message(void) {
  EM_ASM_INT_V({ abort(); });
  while (1);
}

void passert_failed_hashed(uint32_t packed_loghash, ...) {
  EM_ASM_INT_V({ abort(); });
  while (1);
}

void pbl_log(uint8_t log_level, const char* src_filename,
             int src_line_number, const char* fmt, ...) {
  printf("%s:%d ", src_filename, src_line_number);
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}

bool process_manager_compiled_with_legacy2_sdk(void) {
  return false;
}

ResAppNum sys_get_current_resource_num(void) {
  return 1; // 0 is system
}

size_t sys_resource_load_range(ResAppNum app_num, uint32_t id, uint32_t start_bytes,
                               uint8_t *buffer, size_t num_bytes) {
  return emx_resources_read(app_num, id, start_bytes, buffer, num_bytes);
}

size_t sys_resource_size(ResAppNum app_num, uint32_t handle) {
  return emx_resources_get_size(app_num, handle);
}

GFont sys_font_get_system_font(const char *font_key) {
  static FontInfo s_system_fonts_info_table[NUM_SYSTEM_FONTS + 1] = {};

  for (int i = 0; i < (int) NUM_SYSTEM_FONTS; ++i) {
    if (0 == strcmp(font_key, s_font_resource_keys[i].key_name)) {
      FontInfo *fontinfo = &s_system_fonts_info_table[i];
      uint32_t resource = s_font_resource_keys[i].resource_id;
      // if the font has not been initialized yet
      if (!fontinfo->loaded) {
        if (!text_resources_init_font(SYSTEM_APP,
            resource, 0, &s_system_fonts_info_table[i])) {
          // Can't initialize the font for some reason
          return NULL;
        }
      }
      return &s_system_fonts_info_table[i];
    }
  }

  // Didn't find the given font, invalid key.
  return (GFont)NULL;
}

void sys_font_reload_font(FontInfo *fontinfo) {
  text_resources_init_font(fontinfo->base.app_num, fontinfo->base.resource_id,
      fontinfo->extension.resource_id, fontinfo);
}

uint32_t sys_resource_get_and_cache(ResAppNum app_num, uint32_t resource_id) {
  return resource_id;
}

bool sys_resource_is_valid(ResAppNum app_num, uint32_t resource_id) {
  return true;
}

ResourceCallbackHandle resource_watch(ResAppNum app_num,
                                      uint32_t resource_id,
                                      ResourceChangedCallback callback,
                                      void *data) {
  return NULL;
}

void applib_resource_munmap_or_free(void *bytes) {
  free(bytes);
}

void *applib_resource_mmap_or_load(ResAppNum app_num, uint32_t resource_id,
                                   size_t offset, size_t num_bytes, bool used_aligned) {
  if (num_bytes == 0) {
    return NULL;
  }

  uint8_t *result = malloc(num_bytes + (used_aligned ? 7 :0));
  if (!result
      || sys_resource_load_range(app_num, resource_id, offset, result, num_bytes) != num_bytes) {
    free(result);
    return NULL;
  }
  return result;
}

void wtf(void) {
  printf(">>>> WTF\n");
  EM_ASM_INT_V({ abort(); });
  while (1) ;
}

PebbleTask pebble_task_get_current(void) {
  return PebbleTask_App;
}

void app_event_loop(void) {
  // FIXME: PBL-43469 will need to remove this init from here when multiple
  // platform support is implemented.
  rocky_api_watchface_init();
  emx_app_event_loop();
}
