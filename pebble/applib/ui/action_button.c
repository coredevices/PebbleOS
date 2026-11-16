/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "action_button.h"

#include <applib/graphics/graphics.h>
#include <applib/ui/action_bar_layer.h>
#include <applib/ui/action_bar_layer_private.h>
#include <shell/system_theme.h>

void action_button_draw(GContext *ctx, Layer *layer, GColor fill_color) {
  // This should match the window bounds
  const GRect bounds = layer->bounds;
  const bool on_right = action_bar_layer_is_on_right();

  const int radius = ACTION_BUTTON_RADIUS;
  GRect rect = {.size = {radius * 2, radius * 2}};
  grect_align(&rect, &bounds, on_right ? GAlignRight : GAlignLeft, false);

  // Push the button halfway off-screen, and further per default content size
  const int offset = radius + ACTION_BUTTON_EDGE_OFFSET;
  rect.origin.x += on_right ? offset : -offset;

  graphics_context_set_fill_color(ctx, fill_color);
  graphics_fill_oval(ctx, rect, GOvalScaleModeFitCircle);
}

void action_button_update_proc(Layer *action_button_layer, GContext *ctx) {
  action_button_draw(ctx, action_button_layer, system_theme_get_fg_color());
}
