/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "layer.h"

//! @file inverter_layer.h
//! @addtogroup UI
//! @{
//!   @addtogroup Layer Layers
//!   @{
//!     @addtogroup InverterLayer
//! \brief Layer that inverts anything "below it".
//!
//! ![](inverter_layer.png)
//! This layer takes what has been drawn into the graphics context by layers
//! that are "behind" it in the layer hierarchy.
//! Then, the inverter layer uses its geometric information (bounds, frame) as
//! the area to invert in the graphics context. Inverting will cause black
//! pixels to become white and vice versa.
//!
//! The InverterLayer is useful, for example, to highlight the selected item
//! in a menu. In fact, the \ref MenuLayer itself uses InverterLayer to
//! accomplish its selection highlighting.
//!     @{

//! Data structure of an InverterLayer
//! @note an `InverterLayer *` can safely be casted to a `Layer *` and can
//! thus be used with all other functions that take a `Layer *` as an argument.
//! <br/>For example, the following is legal:
//! \code{.c}
//! InverterLayer inverter_layer;
//! ...
//! layer_set_frame((Layer *)&inverter_layer, GRect(10, 10, 50, 50));
//! \endcode
typedef struct InverterLayer {
  Layer layer;
} InverterLayer;

//! Initializes the InverterLayer and resets it to the defaults:
//! * Clips: `true`
//! * Hidden: `false`
//! @param inverter The inverter layer
//! @param frame The frame at which to initialize the layer
void inverter_layer_init(InverterLayer *inverter, const GRect *frame);

//! Creates a new InverterLayer on the heap and initializes it with the default values.
//! * Clips: `true`
//! * Hidden: `false`
//! @return A pointer to the InverterLayer. `NULL` if the InverterLayer could not
//! be created
InverterLayer* inverter_layer_create(GRect frame);

void inverter_layer_deinit(InverterLayer *inverter_layer);

//! Destroys an InverterLayer previously created by inverter_layer_create
void inverter_layer_destroy(InverterLayer* inverter_layer);

//! Gets the "root" Layer of the inverter layer, which is the parent for the sub-
//! layers used for its implementation.
//! @param inverter_layer Pointer to the InverterLayer for which to get the "root" Layer
//! @return The "root" Layer of the inverter layer.
//! @internal
//! @note The result is always equal to `(Layer *) inverter_layer`.
Layer* inverter_layer_get_layer(InverterLayer *inverter_layer);

//!     @} // end addtogroup InverterLayer
//!   @} // end addtogroup Layer
//! @} // end addtogroup UI
