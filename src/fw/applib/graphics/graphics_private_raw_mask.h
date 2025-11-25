/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gtypes.h"

extern const GDrawRawImplementation g_mask_recording_draw_implementation;

void graphics_private_raw_mask_apply(GColor8 *dst_color, const GDrawMask *mask,
                                     unsigned int data_row_offset, int x, int width,
                                     GColor8 src_color);

uint8_t graphics_private_raw_mask_get_value(const GContext *ctx, const GDrawMask *mask, GPoint p);

void graphics_private_raw_mask_set_value(const GContext *ctx, GDrawMask *mask, GPoint p,
                                         uint8_t value);
