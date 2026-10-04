/* SPDX-FileCopyrightText: 2026 Scott Anderson */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Checks one test's screens at every content size the Text Size setting offers in a single
//! reference image: a column per size, a row per screen. A screen identical to an earlier size in
//! its row is labelled with that size. If every size matches, the image is just the first column.
//! Tests that include this list tests/fixtures/screen_grid.c in their sources.

#include <applib/graphics/gtypes.h>
#include <applib/preferred_content_size.h>

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_GRID_MAX_ROWS 2

typedef struct ScreenGrid {
  //! The sizes the Text Size setting offers: one below the default to one above it
  PreferredContentSize first_size;
  PreferredContentSize last_size;
  unsigned int num_rows;
  //! Row 1 is a later moment of row 0, see screen_grid_init_frames()
  bool frames;
  //! One byte per pixel: the screen as rendered, and as shown in the grid if labelled
  uint8_t *rendered[SCREEN_GRID_MAX_ROWS][NumPreferredContentSizes];
  uint8_t *shown[SCREEN_GRID_MAX_ROWS][NumPreferredContentSizes];
} ScreenGrid;

void screen_grid_init(ScreenGrid *grid, unsigned int num_rows);

//! Two rows, the second a later moment of the first, e.g. partway through an animation. A screen
//! that didn't change from the first row is left blank, and if none did the image is just the
//! first row. Add each size's first row screen before its second.
void screen_grid_init_frames(ScreenGrid *grid);

//! Adds the screen just rendered into ctx. A repeat of an earlier size is labelled in ctx.
void screen_grid_add(ScreenGrid *grid, GContext *ctx, PreferredContentSize size, unsigned int row);

//! Checks the grid against the reference image and frees it
void screen_grid_check(ScreenGrid *grid, const char *pbi_file);
