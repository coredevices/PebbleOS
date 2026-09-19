/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/text.h"

#define STUBBED_CREATED_LAYOUT (GTextLayoutCacheRef)123456

void PBL_WEAK graphics_text_layout_cache_init(GTextLayoutCacheRef *layout_cache) {
  *layout_cache = STUBBED_CREATED_LAYOUT;
}

void PBL_WEAK graphics_text_layout_cache_deinit(GTextLayoutCacheRef *layout_cache) {
}
void PBL_WEAK graphics_text_layout_set_line_spacing_delta(GTextLayoutCacheRef layout,
                                                          int16_t delta) {
}

int16_t PBL_WEAK graphics_text_layout_get_line_spacing_delta(const GTextLayoutCacheRef layout) {
  return 0;
}

GSize PBL_WEAK graphics_text_layout_get_max_used_size(GContext *ctx, const char *text,
                                                      GFont const font, const GRect box,
                                                      const GTextOverflowMode overflow_mode,
                                                      const GTextAlignment alignment,
                                                      GTextLayoutCacheRef layout) {
  return GSizeZero;
}

void PBL_WEAK graphics_text_attributes_restore_default_text_flow(GTextLayoutCacheRef layout) {
}

void PBL_WEAK graphics_text_attributes_enable_screen_text_flow(GTextLayoutCacheRef layout,
                                                               uint8_t inset) {
}

void PBL_WEAK graphics_text_attributes_restore_default_paging(GTextLayoutCacheRef layout) {
}

void PBL_WEAK graphics_text_attributes_enable_paging(GTextLayoutCacheRef layout,
                                                     GPoint content_origin_on_screen,
                                                     GRect paging_on_screen) {
}
