/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/layer.h"

//! Action button is the actionable affordance for windows that display actionable content.
//! Action button only provides an update proc instead of entire layer.

//! Draws the action button on the layer with the fill color specified.
//! This expects a layer with a frame and bounds that spans the entire window.
void action_button_draw(GContext *ctx, Layer *layer, GColor fill_color);

//! Update proc of the action button.
//! This expects a layer with a frame and bounds that spans the entire window.
void action_button_update_proc(Layer *action_button_layer, GContext *ctx);
