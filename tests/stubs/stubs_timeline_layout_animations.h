/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/timeline_layout_animations.h"
#include "util/attributes.h"

void WEAK timeline_layout_transition_pin_to_card(TimelineLayout *pin_timeline_layout,
                                                 TimelineLayout *card_timeline_layout) {}

void WEAK timeline_layout_transition_card_to_pin(TimelineLayout *card_timeline_layout,
                                                 TimelineLayout *pin_timeline_layout) {}

Animation *WEAK timeline_layout_create_up_down_animation(
    TimelineLayout *layout, const GRect *from, const GRect *to, const GRect *icon_from,
    const GRect *icon_to, uint32_t duration, InterpolateInt64Function interpolate) {
  return NULL;
}
