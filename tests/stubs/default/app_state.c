/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/app_glances/app_glance_service.h>
#include <pbl/util/heap.h>

#include <applib/graphics/graphics.h>
#include <applib/ui/animation_private.h>
#include <applib/ui/click_internal.h>
#include <applib/ui/layer.h>
#include <applib/ui/window_stack_private.h>
#include <applib/unobstructed_area_service_private.h>
#include <process_state/app_state/app_state.h>

static Heap s_app_heap;

PBL_WEAK bool app_state_configure(MemorySegment *app_state_ram, ProcessAppSDKType sdk_type,
                                  int16_t obstruction_origin_y) {
  return true;
}

PBL_WEAK void app_state_init(void) {
}

PBL_WEAK void app_state_deinit(void) {
}

PBL_WEAK struct pbl_tm *app_state_get_gmtime_tm(void) {
  static struct pbl_tm gmtime_tm = {};
  return &gmtime_tm;
}

PBL_WEAK struct pbl_tm *app_state_get_localtime_tm(void) {
  static struct pbl_tm localtime_tm = {};
  return &localtime_tm;
}

PBL_WEAK char *app_state_get_localtime_zone(void) {
  static char localtime_zone[PBL_TZ_LEN] = {};
  return localtime_zone;
}

PBL_WEAK LocaleInfo *app_state_get_locale_info(void) {
  return nullptr;
}

PBL_WEAK GContext *s_app_state_get_graphics_context;
PBL_WEAK GContext *app_state_get_graphics_context(void) {
  return s_app_state_get_graphics_context;
}

PBL_WEAK Heap *app_state_get_heap(void) {
  return &s_app_heap;
}

static AnimationState s_stub_app_animation_state;

PBL_WEAK AnimationState *app_state_get_animation_state(void) {
  return &s_stub_app_animation_state;
}

static AnimationState s_stub_kernel_animation_state;

PBL_WEAK AnimationState *kernel_applib_get_animation_state(void) {
  return &s_stub_kernel_animation_state;
}

PBL_WEAK GBitmap *app_state_legacy2_get_2bit_framebuffer(void) {
  // Shouldn't be used, only for backwards compatibility
  return nullptr;
}

static Layer *s_layer_tree_stack[LAYER_TREE_STACK_SIZE];

PBL_WEAK Layer **app_state_get_layer_tree_stack(void) {
  return s_layer_tree_stack;
}

PBL_WEAK Layer **kernel_applib_get_layer_tree_stack(void) {
  return s_layer_tree_stack;
}

static WindowStack s_window_stack;

PBL_WEAK WindowStack *app_state_get_window_stack(void) {
  return &s_window_stack;
}

static ClickManager click_manager;

PBL_WEAK ClickManager *app_state_get_click_manager(void) {
  return &click_manager;
}

static void *s_user_data;
PBL_WEAK void app_state_set_user_data(void *data) {
  s_user_data = data;
}

PBL_WEAK void *app_state_get_user_data(void) {
  return s_user_data;
}

static JsRuntimeContext *s_js_runtime_context = nullptr;
static uint8_t *s_runtime_context_buffer = nullptr;
PBL_WEAK void app_state_set_js_runtime_context(uint8_t *unaligned_buffer,
                                               JsRuntimeContext *js_runtime_context) {
  s_js_runtime_context = js_runtime_context;
  s_runtime_context_buffer = unaligned_buffer;
}

PBL_WEAK uint8_t *app_state_get_js_runtime_context_buffer(void) {
  return s_runtime_context_buffer;
}

PBL_WEAK JsRuntimeContext *app_state_get_js_runtime_context(void) {
  return s_js_runtime_context;
}

static JsMemoryAPIContext *s_js_memory_api_context = nullptr;
PBL_WEAK void app_state_set_js_memory_api_context(JsMemoryAPIContext *context) {
  s_js_memory_api_context = context;
}

PBL_WEAK JsMemoryAPIContext *app_state_get_js_memory_api_context(void) {
  return s_js_memory_api_context;
}

PBL_WEAK UnobstructedAreaState s_stub_unobstructed_area_state;

PBL_WEAK UnobstructedAreaState *app_state_get_unobstructed_area_state(void) {
  return &s_stub_unobstructed_area_state;
}

PBL_WEAK AppGlance s_app_glance;

PBL_WEAK AppGlance *app_state_get_glance(void) {
  return &s_app_glance;
}

static bool s_text_perimeter_debugging_enabled;
PBL_WEAK bool app_state_get_text_perimeter_debugging_enabled(void) {
  return s_text_perimeter_debugging_enabled;
}

PBL_WEAK void app_state_set_text_perimeter_debugging_enabled(bool enabled) {
  s_text_perimeter_debugging_enabled = enabled;
}

PBL_WEAK TextRenderState *app_state_get_text_render_state(void) {
  static TextRenderState s_state = {};
  return &s_state;
}

PBL_WEAK FrameBuffer *s_app_state_framebuffer;
FrameBuffer *PBL_WEAK app_state_get_framebuffer(void) {
  return s_app_state_framebuffer;
}
