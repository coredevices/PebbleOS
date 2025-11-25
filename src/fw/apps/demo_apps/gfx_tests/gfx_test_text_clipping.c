/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gfx_tests.h"

static void prv_setup(Window *window);
static void prv_test(Layer *layer, GContext* ctx);
static void prv_teardown(Window *window);

GfxTest g_gfx_test_text_clipping = {
  .name = "Text Clipping",
  .duration = 5,
  .unit_multiple = 0, // Number of characters in test string - set later
  .test_proc = prv_test,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

static GFont s_font;
static Layer s_canvas;

static void prv_setup(Window *window) {
  s_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  layer_init(&s_canvas, &GRect(40, 40, 80, 40));
  layer_add_child(&window->layer, &s_canvas);
}

static void prv_test(Layer *layer, GContext* ctx) {
  const char *text_test_str = "This is a test message that is really long!\"#$%&'()*+,-./01234"
                              "56789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrs"
                              "tuvwxyz{|}";
  g_gfx_test_text_clipping.unit_multiple = strlen(text_test_str);
  GColor color = { .argb = (uint8_t) rand() };
  graphics_context_set_text_color(ctx, color);
  GRect bounds = layer->bounds;
  bounds.origin.y -= 150; // Drop y by 150 pixels so some data gets clipped
  graphics_draw_text(ctx, text_test_str, s_font, bounds,
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void prv_teardown(Window *window) {
  layer_remove_from_parent(&s_canvas);
}
