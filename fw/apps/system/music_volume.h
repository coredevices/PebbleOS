/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/ui/recognizer/recognizer.h"

// Private to Music; angles follow screen coordinates (clockwise is positive).
Recognizer *music_volume_recognizer_create(RecognizerEventCb callback, void *context, GPoint center,
                                           int16_t min_radius, int16_t max_radius);
int32_t music_volume_recognizer_get_steps(const Recognizer *recognizer);
