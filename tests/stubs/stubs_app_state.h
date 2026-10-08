/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

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

bool app_state_configure(MemorySegment *app_state_ram, ProcessAppSDKType sdk_type,
                         int16_t obstruction_origin_y);

void app_state_init(void);

void app_state_deinit(void);

struct pbl_tm *app_state_get_gmtime_tm(void);

struct pbl_tm *app_state_get_localtime_tm(void);

char *app_state_get_localtime_zone(void);

LocaleInfo *app_state_get_locale_info(void);

extern GContext *s_app_state_get_graphics_context;
GContext *app_state_get_graphics_context(void);

Heap *app_state_get_heap(void);

AnimationState *app_state_get_animation_state(void);

AnimationState *kernel_applib_get_animation_state(void);

GBitmap *app_state_legacy2_get_2bit_framebuffer(void);

Layer **app_state_get_layer_tree_stack(void);

Layer **kernel_applib_get_layer_tree_stack(void);

WindowStack *app_state_get_window_stack(void);

ClickManager *app_state_get_click_manager(void);

void app_state_set_user_data(void *data);

void *app_state_get_user_data(void);

void app_state_set_js_runtime_context(uint8_t *unaligned_buffer,
                                      JsRuntimeContext *js_runtime_context);

uint8_t *app_state_get_js_runtime_context_buffer(void);

JsRuntimeContext *app_state_get_js_runtime_context(void);

void app_state_set_js_memory_api_context(JsMemoryAPIContext *context);

JsMemoryAPIContext *app_state_get_js_memory_api_context(void);

extern UnobstructedAreaState s_stub_unobstructed_area_state;

UnobstructedAreaState *app_state_get_unobstructed_area_state(void);

extern AppGlance s_app_glance;

AppGlance *app_state_get_glance(void);

bool app_state_get_text_perimeter_debugging_enabled(void);

void app_state_set_text_perimeter_debugging_enabled(bool enabled);

TextRenderState *app_state_get_text_render_state(void);

extern FrameBuffer *s_app_state_framebuffer;
FrameBuffer *app_state_get_framebuffer(void);
