/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "util/attributes.h"

bool WEAK gcolor_equal(GColor8 x, GColor8 y) {
  return ((x.argb == y.argb) || ((x.a == 0) && (y.a == 0)));
}

GColor8 WEAK gcolor_legible_over(GColor8 background_color) {
  return GColorBlack;
}

