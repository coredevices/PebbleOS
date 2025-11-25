/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "apps/system_apps/timeline/peek_layer.h"
#include "util/attributes.h"

PeekLayer * WEAK peek_layer_create(GRect frame) {
  return NULL;
}

void WEAK peek_layer_destroy(PeekLayer *peek_layer) {}

void WEAK peek_layer_init(PeekLayer *peek_layer, const GRect *frame) {}

void WEAK peek_layer_deinit(PeekLayer *peek_layer) {}

void WEAK peek_layer_set_frame(PeekLayer *peek_layer, const GRect *frame) {}

void WEAK peek_layer_set_icon(PeekLayer *peek_layer, const TimelineResourceInfo *timeline_res) {}

void WEAK peek_layer_set_icon_with_size(PeekLayer *peek_layer,
                                        const TimelineResourceInfo *timeline_res,
                                        TimelineResourceSize res_size, GRect icon_from) {}

void WEAK peek_layer_set_scale_to(PeekLayer *peek_layer, GRect icon_to) {}

void WEAK peek_layer_set_scale_to_image(PeekLayer *peek_layer,
                                        const TimelineResourceInfo *timeline_res,
                                        TimelineResourceSize res_size, GRect icon_to,
                                        bool align_in_frame) {}

void WEAK peek_layer_set_duration(PeekLayer *peek_layer, uint32_t duration);

void WEAK peek_layer_play(PeekLayer *peek_layer) {}

GSize WEAK peek_layer_get_size(PeekLayer *peek_layer) {
  return GSizeZero;
}

ImmutableAnimation * WEAK peek_layer_create_play_animation(PeekLayer *peek_layer) {
  return NULL;
}

ImmutableAnimation * WEAK peek_layer_create_play_section_animation(PeekLayer *peek_layer,
                                                                   uint32_t from_elapsed_ms,
                                                                   uint32_t to_elapsed_ms) {
  return NULL;
}

void WEAK peek_layer_set_background_color(PeekLayer *peek_layer, GColor color) {}

void WEAK peek_layer_set_fields(PeekLayer *peek_layer, const char *number, const char *title,
                                const char *subtitle) {}

void WEAK peek_layer_clear_fields(PeekLayer *peek_layer) {}

void WEAK peek_layer_set_fields_hidden(PeekLayer *peek_layer, bool hidden) {}

void WEAK peek_layer_set_number(PeekLayer *peek_layer, const char *number) {}

void WEAK peek_layer_set_title(PeekLayer *peek_layer, const char *title) {}

void WEAK peek_layer_set_subtitle(PeekLayer *peek_layer, const char *subtitle) {}

void WEAK peek_layer_set_title_font(PeekLayer *peek_layer, GFont font) {}

void WEAK peek_layer_set_subtitle_font(PeekLayer *peek_layer, GFont font, int16_t margin) {}

void WEAK peek_layer_set_dot_diameter(PeekLayer *peek_layer, uint8_t dot_diameter) {}

void WEAK peek_layer_set_icon_offset_y(PeekLayer *peek_layer, int16_t icon_offset_y) {}
