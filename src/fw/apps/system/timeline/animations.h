/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/animation.h"

#define TIMELINE_NUM_MOOOK_FRAMES_MID 3
#define TIMELINE_UP_DOWN_ANIMATION_DURATION_MS \
    (interpolate_moook_soft_duration(TIMELINE_NUM_MOOOK_FRAMES_MID))

int64_t timeline_animation_interpolate_moook_soft(int32_t normalized,
                                                  int64_t from, int64_t to);

int64_t timeline_animation_interpolate_moook_second_half(int32_t normalized,
                                                         int64_t from, int64_t to);

void timeline_animation_layer_stopped_cut_to_end(Animation *animation, bool finished,
                                                 void *context);
