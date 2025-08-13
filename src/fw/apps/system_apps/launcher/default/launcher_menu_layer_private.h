/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
