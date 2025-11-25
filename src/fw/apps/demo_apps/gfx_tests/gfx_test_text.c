/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gfx_tests.h"

static void prv_setup(Window *window);
static void prv_test(Layer *layer, GContext* ctx);

GfxTest g_gfx_test_text = {
  .name = "Text",
  .duration = 5,
  .unit_multiple = 0,  // Number of characters in test string - set later
  .test_proc = prv_test,
  .setup = prv_setup,
};

static GFont s_font;

static void prv_setup(Window *window) {
  s_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

static void prv_test(Layer *layer, GContext* ctx) {
  const char *text_test_str = "Lorem ipsum dolor sit amet, ne choro argumentum est, quando latine "
                              "copiosae est ea, usu nonumes accusam te.";
  g_gfx_test_text.unit_multiple = strlen(text_test_str);
  GColor color = { .argb = (uint8_t) rand() };
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text_test_str, s_font, layer->bounds,
      GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}
