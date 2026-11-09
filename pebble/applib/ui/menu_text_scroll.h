/* SPDX-FileCopyrightText: 2026 Scott Anderson */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <applib/graphics/graphics.h>
#include <applib/ui/layer.h>

//! Horizontal scrolling of overlong single line text in the highlighted cell of system menus.
//! There is one scroll per task, following whichever highlighted cell is being drawn.

//! @return Whether menus drawn by the calling process may scroll their text. Third-party apps keep
//! ellipsized text.
bool menu_text_scroll_is_allowed(void);

//! @return How many pixels wider than \a box the text is when laid out on one line
int16_t menu_text_scroll_get_overflow(GContext *ctx, const char *text, GFont font,
                                      const GRect *box);

//! Called while drawing a highlighted cell whose longest text is \a overflow pixels wider than the
//! room it has. Keeps the menu redrawing while the text moves.
//! @return How far to shift the text left
int16_t menu_text_scroll_get_offset(const Layer *cell_layer, int16_t overflow);

//! Draws left aligned text shifted left by \a offset and clipped to \a box, or normally when it is
//! not scrolled.
void menu_text_scroll_draw_text(GContext *ctx, const char *text, GFont font, const GRect *box,
                                GTextOverflowMode overflow_mode, int16_t overflow, int16_t offset);

//! Stops scrolling text of the menu whose cells are children of \a content_layer.
void menu_text_scroll_layer_deinit(const Layer *content_layer);
