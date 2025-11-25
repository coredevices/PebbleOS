/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/menu_layer.h"
#include "applib/graphics/gtypes.h"

#define MENU_CELL_LEGACY2_BASIC_SEPARATOR_HEIGHT ((const int16_t) 1)

//! Data structure containing all the callbacks of a MenuLayer.
typedef struct MenuLayerCallbacksLegacy2 {
  //! Callback that gets called to get the number of sections in the menu.
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the number of sections defaults to 1.
  MenuLayerGetNumberOfSectionsCallback get_num_sections;

  //! Callback that gets called to get the number of rows in a section. This
  //! can get called at various moments throughout the life of a menu.
  //! @note Must be set to a valid callback; `NULL` causes undefined behavior.
  MenuLayerGetNumberOfRowsInSectionsCallback get_num_rows;

  //! Callback that gets called to get the height of a cell.
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the default height of 44 pixels is used.
  MenuLayerGetCellHeightCallback get_cell_height;

  //! Callback that gets called to get the height of a section header.
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the defaults height of 0 pixels is used. This disables
  //! section headers.
  MenuLayerGetHeaderHeightCallback get_header_height;

  //! Callback that gets called to render a menu item.
  //! This gets called for each menu item, every time it needs to be
  //! re-rendered.
  //! @note Must be set to a valid callback; `NULL` causes undefined behavior.
  MenuLayerDrawRowCallback draw_row;

  //! Callback that gets called to render a section header.
  //! This gets called for each section header, every time it needs to be
  //! re-rendered.
  //! @note Must be set to a valid callback, unless `.get_header_height` is
  //! `NULL`. Causes undefined behavior otherwise.
  MenuLayerDrawHeaderCallback draw_header;

  //! Callback that gets called when the user triggers a click with the SELECT
  //! button.
  //! @note When `NULL`, click events for the SELECT button are ignored.
  MenuLayerSelectCallback select_click;

  //! Callback that gets called when the user triggers a long click with the
  //! SELECT button.
  //! @note When `NULL`, long click events for the SELECT button are ignored.
  MenuLayerSelectCallback select_long_click;

  //! Callback that gets called whenever the selection changes.
  //! @note When `NULL`, selection change events are ignored.
  MenuLayerSelectionChangedCallback selection_changed;

  //! Callback that gets called to get the height of a separator
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the default height of 1 is used.
  MenuLayerGetSeparatorHeightCallback get_separator_height;

  //! Callback that gets called to render a separator.
  //! This gets called for each separator, every time it needs to be
  //! re-rendered.
  //! @note Must be set to a valid callback, unless `.get_separator_height` is
  //! `NULL`. Causes undefined behavior otherwise.
  MenuLayerDrawSeparatorCallback draw_separator;
} MenuLayerCallbacksLegacy2;

typedef struct MenuLayerCallbacksLegacy2__deprecated {
  //! Callback that gets called to get the number of sections in the menu.
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the number of sections defaults to 1.
  MenuLayerGetNumberOfSectionsCallback get_num_sections;

  //! Callback that gets called to get the number of rows in a section. This
  //! can get called at various moments throughout the life of a menu.
  //! @note Must be set to a valid callback; `NULL` causes undefined behavior.
  MenuLayerGetNumberOfRowsInSectionsCallback get_num_rows;

  //! Callback that gets called to get the height of a cell.
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the default height of 44 pixels is used.
  MenuLayerGetCellHeightCallback get_cell_height;

  //! Callback that gets called to get the height of a section header.
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the defaults height of 0 pixels is used. This disables
  //! section headers.
  MenuLayerGetHeaderHeightCallback get_header_height;

  //! Callback that gets called to render a menu item.
  //! This gets called for each menu item, every time it needs to be
  //! re-rendered.
  //! @note Must be set to a valid callback; `NULL` causes undefined behavior.
  MenuLayerDrawRowCallback draw_row;

  //! Callback that gets called to render a section header.
  //! This gets called for each section header, every time it needs to be
  //! re-rendered.
  //! @note Must be set to a valid callback, unless `.get_header_height` is
  //! `NULL`. Causes undefined behavior otherwise.
  MenuLayerDrawHeaderCallback draw_header;

  //! Callback that gets called when the user triggers a click with the SELECT
  //! button.
  //! @note When `NULL`, click events for the SELECT button are ignored.
  MenuLayerSelectCallback select_click;

  //! Callback that gets called when the user triggers a long click with the
  //! SELECT button.
  //! @note When `NULL`, long click events for the SELECT button are ignored.
  MenuLayerSelectCallback select_long_click;

  //! Callback that gets called whenever the selection changes.
  //! @note When `NULL`, selection change events are ignored.
  MenuLayerSelectionChangedCallback selection_changed;

  //! Callback that gets called to get the height of a separator
  //! This can get called at various moments throughout the life of a menu.
  //! @note When `NULL`, the default height of 1 is used.
  MenuLayerGetSeparatorHeightCallback get_separator_height;

  //! Callback that gets called to render a separator.
  //! This gets called for each separator, every time it needs to be
  //! re-rendered.
  //! @note Must be set to a valid callback, unless `.get_separator_height` is
  //! `NULL`. Causes undefined behavior otherwise.
  MenuLayerDrawSeparatorCallback draw_separator;
} MenuLayerCallbacksLegacy2__deprecated;

void menu_layer_legacy2_init(MenuLayer *menu_layer, const GRect *frame);

MenuLayer* menu_layer_legacy2_create(GRect frame);

void menu_layer_legacy2_set_callbacks(MenuLayer *menu_layer,
                                      void *callback_context,
                                      MenuLayerCallbacksLegacy2 callbacks);

void menu_layer_legacy2_set_callbacks__deprecated(MenuLayer *menu_layer,
                                                  void *callback_context,
                                                  MenuLayerCallbacksLegacy2__deprecated callbacks);
