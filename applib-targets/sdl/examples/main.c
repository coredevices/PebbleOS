/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "sdl_graphics.h"
#include "sdl_app.h"

#include "applib/app.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_line.h"


int main(void) {
  GContext *context = sdl_graphics_get_gcontext();
  graphics_context_set_stroke_color(context, GColorBrightGreen);
  graphics_context_set_stroke_width(context, 2);
  graphics_draw_line(context, (GPoint){0, 0}, (GPoint){100, 100});
  graphics_draw_line(context, (GPoint){0, 10}, (GPoint){100, 10});
  graphics_draw_line(context, (GPoint){0, 20}, (GPoint){100, 20});
  graphics_draw_line(context, (GPoint){0, 30}, (GPoint){100, 30});
  graphics_draw_circle(context, (GPoint){50, 50}, 20);
  app_event_loop();

  return 0;
}
