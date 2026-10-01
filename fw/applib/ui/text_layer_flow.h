/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "text_layer.h"

//! @internal
//! Default value for paging if we cannot find a container layer (e.g. ScrollLayer).
//! The value is very large but small enough to prevent overflows in the math
//! in case the layer's origin is > 0
#define TEXT_LAYER_FLOW_DEFAULT_PAGING_HEIGHT (INT16_MAX / 2)

//! @internal
bool text_layer_calc_text_flow_paging_values(const TextLayer *text_layer,
                                             GPoint *content_origin_on_screen,
                                             GRect *page_rect_on_screen);
