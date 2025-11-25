/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/status_bar_layer.h"

void status_bar_layer_render(GContext *ctx, const GRect *bounds, StatusBarLayerConfig *config) {}

void status_bar_layer_init(StatusBarLayer *status_bar_layer) {}

void status_bar_layer_set_colors(StatusBarLayer *status_bar_layer, GColor background,
                                 GColor foreground) {}

void status_bar_layer_deinit(StatusBarLayer *status_bar_layer) {}

void status_bar_layer_set_separator_mode(StatusBarLayer *status_bar_layer,
                                         StatusBarLayerSeparatorMode mode) {}
