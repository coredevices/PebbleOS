/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define DISPLAY_ORIENTATION_COLUMN_MAJOR_INVERTED 0
#define DISPLAY_ORIENTATION_ROTATED_180 1
#define DISPLAY_ORIENTATION_ROW_MAJOR 0
#define DISPLAY_ORIENTATION_ROW_MAJOR_INVERTED 1

#define PBL_BW 0
#define PBL_COLOR 1

#define PBL_RECT 0
#define PBL_ROUND 1

#define PBL_DISPLAY_WIDTH 260
#define PBL_DISPLAY_HEIGHT 260

// Getafix doesn't support 2x apps, but need to define these anyways so it builds
#define LEGACY_2X_DISP_COLS 180
#define LEGACY_2X_DISP_ROWS 180
#define LEGACY_3X_DISP_COLS 180
#define LEGACY_3X_DISP_ROWS 180

// Use rectangular framebuffer to enable in-place pixel format conversion in display driver
// This uses more memory than circular (67600 vs 53786) but saves the separate display buffer
#define DISPLAY_FRAMEBUFFER_BYTES (PBL_DISPLAY_WIDTH * PBL_DISPLAY_HEIGHT)

extern const void * const g_gbitmap_getafix_data_row_infos;
extern const void * const g_gbitmap_getafix_legacy_3x_data_row_infos;