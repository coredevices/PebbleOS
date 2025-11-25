/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "util/heap.h"
#include "util/circular_cache.h"

#include "sdl_app.h"
#include "sdl_graphics.h"

void *task_malloc(size_t bytes) {
  return malloc(bytes);
}

void task_free(void *ptr) {
  free(ptr);
}

void app_log(uint8_t log_level, const char* src_filename,
             int src_line_number, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

GContext* app_state_get_graphics_context() {
  return sdl_graphics_get_gcontext();
}

Heap *app_state_get_heap(void) {
  return NULL;
}

GBitmap* app_state_legacy2_get_2bit_framebuffer(void) {
  return NULL;
}

void circular_cache_init(CircularCache* c, uint8_t* buffer, size_t item_size,
    int total_items, Comparator compare_cb) {
}

bool heap_is_allocated(Heap* const heap, void* ptr) {
  return false;
}

void passert_failed(const char* filename, int line_number, const char* message, ...) {
}

void passert_failed_no_message(const char* filename, int line_number) {
}

void pbl_log(uint8_t log_level, const char* src_filename,
             int src_line_number, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

bool process_manager_compiled_with_legacy2_sdk(void) {
  return false;
}

ResAppNum sys_get_current_resource_num(void) {
  return 0;
}

const uint8_t * sys_resource_builtin_bytes(ResAppNum app_num, uint32_t resource_id,
                                           uint32_t *num_bytes_out) {
  return 0;
}

size_t sys_resource_load_range(ResAppNum app_num, uint32_t id, uint32_t start_bytes,
                               uint8_t *buffer, size_t num_bytes) {
  return 0;
}

size_t sys_resource_size(ResAppNum app_num, uint32_t handle) {
  return 0;
}

void app_event_loop(void) {
  sdl_app_event_loop();
}

void wtf(void) {
  printf(">>> WTF\n");
}
