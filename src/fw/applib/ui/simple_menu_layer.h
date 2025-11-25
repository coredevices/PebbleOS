/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/ui/menu_layer.h"
#include "applib/ui/window.h"

//! @file simple_menu_layer.h
//! @addtogroup UI
//! @{
//!   @addtogroup Layer Layers
//!   @{
//!     @addtogroup SimpleMenuLayer
//! \brief Wrapper around \ref MenuLayer, that uses static data to display a
//! list menu.
//!
//! ![](simple_menu_layer.png)
//!     @{

//! Function signature for the callback to handle the event that a user hits
//! the SELECT button.
//! @param index The row index of the item
//! @param context The callback context
typedef void (*SimpleMenuLayerSelectCallback)(int index, void *context);

//! Data structure containing the information of a menu item.
typedef struct {
  //! The title of the menu item. Required.
  const char *title;
  //! The subtitle of the menu item. Optional, leave `NULL` if unused.
  const char *subtitle;
  //! The icon of the menu item. Optional, leave `NULL` if unused.
  GBitmap *icon;
  //! The callback that needs to be called upon a click on the SELECT button.
  //! Optional, leave `NULL` if unused.
  SimpleMenuLayerSelectCallback callback;
} SimpleMenuItem;

//! Data structure containing the information of a menu section.
typedef struct {
  //! Title of the section. Optional, leave `NULL` if unused.
  const char *title;
  //! Array of items in the section.
  const SimpleMenuItem *items;
  //! Number of items in the `.items` array.
  uint32_t num_items;
} SimpleMenuSection;

//! Data structure of a SimpleMenuLayer.
//! @note a `SimpleMenuLayer *` can safely be casted to a `Layer *` and to a
//! `MenuLayer *` and can thus be used with all other functions that take a
//! `Layer *` or `MenuLayer *`, respectively, as an argument.
//! <br/>For example, the following is legal:
//! \code{.c}
//! SimpleMenuLayer simple_menu_layer;
//! ...
//! layer_set_hidden((Layer *)&simple_menu_layer, true);
//! \endcode
//! @note However there are a few caveats:
//! * Do not try to change to bounds or frame of a simple menu layer, after
//! initializing it.
typedef struct {
  MenuLayer menu;

  const SimpleMenuSection *sections;
  int32_t num_sections;
  void *callback_context;
} SimpleMenuLayer;

//! Initializes a SimpleMenuLayer at given frame and with given data.
//! It also sets the internal click configuration provider onto given window.
//! @param simple_menu Pointer to the SimpleMenuLayer to initialize
//! @param frame The frame at which to initialize the menu
//! @param window The window onto which to set the click configuration provider
//! @param sections Array with sections that need to be displayed in the menu
//! @param num_sections The number of sections in the `sections` array.
//! @param callback_context Pointer to application specific data, that is passed
//! into the callbacks.
//! @note The `sections` array is not deep-copied and can therefore not be stack
//! allocated, but needs to be backed by long-lived storage.
//! @note This function does not add the menu's layer to the window.
void simple_menu_layer_init(SimpleMenuLayer *simple_menu, const GRect *frame, Window *window,
                            const SimpleMenuSection *sections, int num_sections,
                            void *callback_context);

//! Creates a new SimpleMenuLayer on the heap and initializes it.
//! It also sets the internal click configuration provider onto given window.
//! @param frame The frame at which to initialize the menu
//! @param window The window onto which to set the click configuration provider
//! @param sections Array with sections that need to be displayed in the menu
//! @param num_sections The number of sections in the `sections` array.
//! @param callback_context Pointer to application specific data, that is passed
//! into the callbacks.
//! @note The `sections` array is not deep-copied and can therefore not be stack
//! allocated, but needs to be backed by long-lived storage.
//! @note This function does not add the menu's layer to the window.
//! @return A pointer to the SimpleMenuLayer. `NULL` if the SimpleMenuLayer could not
//! be created
SimpleMenuLayer* simple_menu_layer_create(GRect frame, Window *window,
    const SimpleMenuSection *sections, int32_t num_sections, void *callback_context);

void simple_menu_layer_deinit(SimpleMenuLayer *menu_layer);

//! Destroys a SimpleMenuLayer previously created by simple_menu_layer_create.
void simple_menu_layer_destroy(SimpleMenuLayer* menu_layer);

//! Gets the "root" Layer of the simple menu layer, which is the parent for the
//! sub-layers used for its implementation.
//! @param simple_menu Pointer to the SimpleMenuLayer for which to get the
//! "root" Layer
//! @return The "root" Layer of the menu layer.
//! @internal
//! @note The result is always equal to `(Layer *) simple_menu`.
Layer* simple_menu_layer_get_layer(const SimpleMenuLayer *simple_menu);

//! Gets the row index of the currently selection menu item.
//! @param simple_menu The SimpleMenuLayer for which to get the current
//! selected row index.
int simple_menu_layer_get_selected_index(const SimpleMenuLayer *simple_menu);

//! Selects the item in the first section at given row index.
//! @param simple_menu The SimpleMenuLayer for which to change the selection
//! @param index The row index of the item to select
//! @param animated Supply `true` to animate changing the selection, or `false`
//! to change the selection instantly.
void simple_menu_layer_set_selected_index(SimpleMenuLayer *simple_menu, int32_t index, bool animated);

//! @param simple_menu The \ref SimpleMenuLayer to get the \ref MenuLayer from.
//! @return The \ref MenuLayer.
MenuLayer *simple_menu_layer_get_menu_layer(SimpleMenuLayer *simple_menu);

//!     @} // end addtogroup SimpleMenuLayer
//!   @} // end addtogroup Layer
//! @} // end addtogroup UI
