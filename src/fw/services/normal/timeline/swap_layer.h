/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "applib/ui/layer.h"
#include "applib/ui/click.h"
#include "applib/ui/property_animation.h"
#include "applib/graphics/gpath.h"
#include "services/normal/timeline/layout_layer.h"

struct Window;
struct SwapLayer;

//! Function signature for the `.get_layout_handler` callback.
typedef LayoutLayer* (*SwapLayerGetLayoutHandler)(struct SwapLayer *swap_layer, int8_t rel_position,
    void *context);

//! Function signature for the `.layout_removed_handler` callback.
typedef void (*SwapLayerLayoutRemovedHandler)(struct SwapLayer *swap_layer, LayoutLayer *layer,
    void *context);

//! Function signature for the `.layout_did_appear_handler` callback.
typedef void (*SwapLayerLayoutDidAppearHandler)(struct SwapLayer *swap_layer, LayoutLayer *layer,
    int8_t rel_change, void *context);

//! Function signature for the `.layout_will_appear_handler` callback.
typedef void (*SwapLayerLayoutWillAppearHandler)(struct SwapLayer *swap_layer, LayoutLayer *layer,
    void *context);

//! Function signature for the `.update_colors_handler` callback.
typedef void (*SwapLayerUpdateColorsHandler)(struct SwapLayer *swap_layer, GColor bg_color,
    bool status_bar_filled, void *context);

//! Function signature for the `.interaction_handler` callback.
typedef void (*SwapLayerInteractionHandler)(struct SwapLayer *swap_layer, void *context);

//! All the callbacks that the SwapLayer exposes for use by applications.
//! @note The context parameter can be set using swap_layer_set_context() and
//! gets passed in as context with all of these callbacks.
typedef struct {
  SwapLayerGetLayoutHandler get_layout_handler;
  SwapLayerLayoutRemovedHandler layout_removed_handler;
  SwapLayerLayoutDidAppearHandler layout_did_appear_handler;
  SwapLayerLayoutWillAppearHandler layout_will_appear_handler;
  SwapLayerUpdateColorsHandler update_colors_handler;
  SwapLayerInteractionHandler interaction_handler;
  ClickConfigProvider click_config_provider;
} SwapLayerCallbacks;

typedef struct {
  Layer layer;
  GBitmap arrow_bitmap;
} ArrowLayer;

//! Data structure of a SwapLayer
//! @note a `SwapLayer *` can safely be casted to a `Layer *` and can thus be
//! used with all other functions that take a `Layer *` as an argument.
//! <br/>For example, the following is legal:
//! \code{.c}
//! SwapLayer swap_layer;
//! ...
//! layer_set_hidden((Layer *)&swap_layer, true);
//! \endcode
//! @note However, there are a few caveats:
//! * To add content layers, you must use \ref swap_layer_add_child().
//! * To change the frame of a scroll layer, use \ref swap_layer_set_frame().
typedef struct SwapLayer {
  Layer layer;
  ArrowLayer arrow_layer;
  Animation *animation;
  LayoutLayer *previous; //!< Previous LayoutLayer in the list.
  LayoutLayer *current; //!< Current LayoutLayer in the list.
  LayoutLayer *next; //!< Next LayoutLayer in the list.
  SwapLayerCallbacks callbacks;
  uint16_t swap_delay_remaining;
  bool swap_in_progress;
  bool is_deiniting;
  void *context;
} SwapLayer;

//! Init. Contains no layouts at this point.
void swap_layer_init(SwapLayer *swap_layer, const GRect *frame);

//! Deinits a SwapLayer and will call the .layout_removed_handler for all layers currently being
//! tracked by the SwapLayer
void swap_layer_deinit(SwapLayer *swap_layer);

//! Calls the .layout_removed_handler for each layout currently known by the SwapLayer,
//! then fetches the "current" and "next" layouts.
//! The callbacks "layout_will_appear" and "layout_did_appear" will both be called.
void swap_layer_reload_data(SwapLayer *swap_layer);

//! Returns the currently focused LayoutLayer of the SwapLayer.
LayoutLayer *swap_layer_get_current_layout(const SwapLayer *swap_layer);

Layer *swap_layer_get_layer(const SwapLayer *swap_layer);

void swap_layer_set_callbacks(SwapLayer *swap_layer, void *callback_context,
                              SwapLayerCallbacks callbacks);

void swap_layer_set_click_config_onto_window(SwapLayer *swap_layer, struct Window *window);

//! Will attempt to swap layers in the given "direction".
//! This will fail if there are no layouts to swap to. The client will know if it succeeded by
//! whether it got a "layout_will_appear" and "layout_did_appear" event.
//! Returns whether the swap attempt was successful.
bool swap_layer_attempt_layer_swap(SwapLayer *swap_layer, ScrollDirection direction);
