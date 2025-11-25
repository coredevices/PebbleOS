/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math.h"

#if PLATFORM_ROBERT || PLATFORM_OBELIX
#define LAUNCHER_MENU_LAYER_CELL_RECT_CELL_HEIGHT (53)
#else
#define LAUNCHER_MENU_LAYER_CELL_RECT_CELL_HEIGHT (42)
#endif

#define LAUNCHER_MENU_LAYER_CELL_ROUND_FOCUSED_CELL_HEIGHT (52)
#define LAUNCHER_MENU_LAYER_CELL_ROUND_UNFOCUSED_CELL_HEIGHT (38)

#if PBL_ROUND
//! Two "unfocused" cells above and below one centered "focused" cell
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS (3)
#else
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS \
    (DIVIDE_CEIL(DISP_ROWS, LAUNCHER_MENU_LAYER_CELL_RECT_CELL_HEIGHT))
#endif
