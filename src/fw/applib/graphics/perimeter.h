/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"

typedef struct GPerimeter GPerimeter;

//! @internal
typedef GRangeHorizontal (*GPerimeterCallback)(const GPerimeter *perimeter, const GSize *ctx_size,
                                               GRangeVertical vertical_range, uint16_t inset);

//! @internal
typedef struct GPerimeter {
  GPerimeterCallback callback;
} GPerimeter;

//! @internal
extern const GPerimeter * const g_perimeter_for_display;
