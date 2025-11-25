/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define DISPLAY_ORIENTATION_COLUMN_MAJOR_INVERTED 0
#define DISPLAY_ORIENTATION_ROTATED_180 0
#define DISPLAY_ORIENTATION_ROW_MAJOR 1
#define DISPLAY_ORIENTATION_ROW_MAJOR_INVERTED 0

#define PBL_BW 0
#define PBL_COLOR 1

#define PBL_RECT 0
#define PBL_ROUND 1

#define PBL_DISPLAY_WIDTH 180
#define PBL_DISPLAY_HEIGHT 180

// Spalding doesn't support 2x apps, but need to define these anyways so it builds
#define LEGACY_2X_DISP_COLS DISP_COLS
#define LEGACY_2X_DISP_ROWS DISP_ROWS
#define LEGACY_3X_DISP_COLS DISP_COLS
#define LEGACY_3X_DISP_ROWS DISP_ROWS

// all pixels + 76 padding pixels before the first/after the last row
#define DISPLAY_FRAMEBUFFER_BYTES 25944

extern const void * const g_gbitmap_spalding_data_row_infos;
