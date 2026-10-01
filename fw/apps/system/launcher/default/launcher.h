/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "../launcher.h"

#include "menu_layer.h"

#include "applib/graphics/gtypes.h"

#include <stdbool.h>

typedef struct LauncherMenuArgs {
  bool reset_scroll;
} LauncherMenuArgs;

typedef struct LauncherDrawState {
  GRangeVertical selection_vertical_range;
  GColor selection_background_color;
} LauncherDrawState;

const LauncherDrawState *launcher_app_get_draw_state(void);
