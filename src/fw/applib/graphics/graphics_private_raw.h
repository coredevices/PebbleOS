/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gtypes.h"

extern const GDrawRawImplementation g_default_draw_implementation;

void graphics_private_raw_blend_color_factor(const GContext *ctx, GColor *dst_color,
                                             unsigned int data_offset,
                                             GColor src_color, int x,
                                             uint8_t factor);
