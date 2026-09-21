/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics.h"

#include <stdint.h>

void health_weight_entry_ui_draw(GContext *ctx, const GRect *bounds, int32_t value_tenths,
                                 int32_t min_tenths, int32_t max_tenths, const char *title,
                                 const char *unit, const char *value_font_key);
