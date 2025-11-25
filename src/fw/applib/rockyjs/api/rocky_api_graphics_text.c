/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <applib/graphics/gcontext.h>
#include <applib/graphics/gtypes.h>
#include "rocky_api_graphics_text.h"

#include "applib/fonts/fonts.h"
#include "kernel/pbl_malloc.h"
#include "rocky_api_graphics.h"
#include "rocky_api_util_args.h"
#include "rocky_api_util.h"

#include <math.h>
#include <string.h>

#define ROCKY_CONTEXT2D_FILLTEXT "fillText"
#define ROCKY_CONTEXT2D_FONT "font"
#define ROCKY_CONTEXT2D_MEASURETEXT "measureText"
#define ROCKY_CONTEXT2D_TEXTALIGN "textAlign"

// TODO: PBL-35780 use app_state_get_rocky_runtime_context().context_binding instead
SECTION(".rocky_bss") T_STATIC RockyAPITextState s_rocky_text_state;
SECTION(".rocky_bss") static GFont s_default_font;


static GSize prv_get_max_used_size(GContext *ctx, const char *str_buffer, const GRect box) {
  return graphics_text_layout_get_max_used_size(ctx, str_buffer,
                                                s_rocky_text_state.font,
                                                box, s_rocky_text_state.overflow_mode,
                                                s_rocky_text_state.alignment, NULL);
}

// fillText(text, x, y [, maxWidth])
JERRY_FUNCTION(prv_fill_text) {
  char *str_buffer;
  int16_t x;
  int16_t y;
  // we don't use INT16_MAX as this seems to leads to overflows deep down in our code
  const int16_t large_int = 10000;
  int16_t box_width;


  ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(
    ROCKY_ARG(str_buffer),
    ROCKY_ARG(x),
    ROCKY_ARG(y),
  );
  if (argc >= 4) {
    // we use this to get range checks and rounding for free
    JS_VAR result = rocky_args_assign(argc - 3, &argv[3],
        &(RockyArgBinding){.ptr = &box_width, .type = RockyArgTypeInt16}, 1);
    if (jerry_value_has_error_flag(result)) {
      return jerry_acquire_value(result);
    }
  } else {
    box_width = large_int;
  }

  GContext *const ctx = rocky_api_graphics_get_gcontext();
  GRect box = {
    .origin.x = x,
    .origin.y = y,
    .size.w = box_width,
    .size.h = large_int,
  };

  // adjust box to accommodate for alignment
  switch (s_rocky_text_state.alignment) {
    case GTextAlignmentCenter: {
      box.origin.x -= box.size.w / 2;
      break;
    }
    case GTextAlignmentRight: {
      box.origin.x -= box.size.w;
      break;
    }
    default: {} // do nothing
  }

  ctx->draw_state.text_color = ctx->draw_state.fill_color;
  graphics_draw_text(ctx, str_buffer, s_rocky_text_state.font,
                     box,
                     s_rocky_text_state.overflow_mode,
                     s_rocky_text_state.alignment,
                     s_rocky_text_state.text_attributes);

  task_free(str_buffer);

  return jerry_create_undefined();
}

static bool prv_text_align_from_value(jerry_value_t value, GTextAlignment *result) {
  char str[10] = {0};
  jerry_string_to_utf8_char_buffer(value, (jerry_char_t *)str, sizeof(str));

  #define HANDLE_CASE(identifer, value) \
    if (strcmp(identifer, str) == 0) { \
      *result = value; \
      return true; \
    }

  HANDLE_CASE("left", GTextAlignmentLeft);
  HANDLE_CASE("right", GTextAlignmentRight);
  HANDLE_CASE("center", GTextAlignmentCenter);
  // assuming left-to-right
  HANDLE_CASE("start", GTextAlignmentLeft);
  HANDLE_CASE("end", GTextAlignmentRight);

  #undef HANDLE_CASE

  // unknown value
  return false;
}

JERRY_FUNCTION(prv_set_text_align) {
  GTextAlignment alignment;
  if (argc >= 1 && prv_text_align_from_value(argv[0], &alignment)) {
    s_rocky_text_state.alignment = alignment;
  }
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_get_text_align) {
  char *align_str = NULL;
  switch (s_rocky_text_state.alignment) {
    case GTextAlignmentLeft:
      align_str = "left";
      break;
    case GTextAlignmentRight:
      align_str = "right";
      break;
    case GTextAlignmentCenter:
      align_str = "center";
      break;
  }

  return jerry_create_string((const jerry_char_t *)align_str);
}

// we speed this up, e.g. by sorting and doing binary search if this ever becomes an issue
T_STATIC const RockyAPISystemFontDefinition s_font_definitions[] = {
  {.js_name = "18px bold Gothic", .res_key = FONT_KEY_GOTHIC_18_BOLD},
  {.js_name = "14px Gothic", .res_key = FONT_KEY_GOTHIC_14},
  {.js_name = "14px bold Gothic", .res_key = FONT_KEY_GOTHIC_14_BOLD},
  {.js_name = "18px Gothic", .res_key = FONT_KEY_GOTHIC_18},
  {.js_name = "24px Gothic", .res_key = FONT_KEY_GOTHIC_24},
  {.js_name = "24px bold Gothic", .res_key = FONT_KEY_GOTHIC_24_BOLD},
  {.js_name = "28px Gothic", .res_key = FONT_KEY_GOTHIC_28},
  {.js_name = "28px bold Gothic", .res_key = FONT_KEY_GOTHIC_28_BOLD},
  {.js_name = "30px bolder Bitham", .res_key = FONT_KEY_BITHAM_30_BLACK},
  {.js_name = "42px bold Bitham", .res_key = FONT_KEY_BITHAM_42_BOLD},
  {.js_name = "42px light Bitham", .res_key = FONT_KEY_BITHAM_42_LIGHT},
  {.js_name = "42px Bitham-numeric", .res_key = FONT_KEY_BITHAM_42_MEDIUM_NUMBERS},
  {.js_name = "34px Bitham-numeric", .res_key = FONT_KEY_BITHAM_34_MEDIUM_NUMBERS},
  {.js_name = "21px Roboto", .res_key = FONT_KEY_ROBOTO_CONDENSED_21},
  {.js_name = "49px Roboto-subset", .res_key = FONT_KEY_ROBOTO_BOLD_SUBSET_49},
  {.js_name = "28px bold Droid-serif", .res_key = FONT_KEY_DROID_SERIF_28_BOLD},
  {.js_name = "20px bold Leco-numbers", .res_key = FONT_KEY_LECO_20_BOLD_NUMBERS},
  {.js_name = "26px bold Leco-numbers-am-pm", .res_key = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM},
  {.js_name = "32px bold numbers Leco-numbers", .res_key = FONT_KEY_LECO_32_BOLD_NUMBERS},
  {.js_name = "36px bold numbers Leco-numbers", .res_key = FONT_KEY_LECO_36_BOLD_NUMBERS},
  {.js_name = "38px bold numbers Leco-numbers", .res_key = FONT_KEY_LECO_38_BOLD_NUMBERS},
  {.js_name = "42px bold numbers Leco-numbers", .res_key = FONT_KEY_LECO_42_NUMBERS},
  {.js_name = "28px light numbers Leco-numbers", .res_key = FONT_KEY_LECO_28_LIGHT_NUMBERS},
  { 0 }, // element to support unit-testing
};

//! The index to the default font in s_font_definitions
#define DEFAULT_FONT_DEFINITION (s_font_definitions[2])

T_STATIC bool prv_font_definition_from_value(
      jerry_value_t value, RockyAPISystemFontDefinition const **result) {
  char str[50] = {0};
  jerry_string_to_utf8_char_buffer(value, (jerry_char_t *)str, sizeof(str));

  const RockyAPISystemFontDefinition *def = s_font_definitions;
  while (def->js_name) {
    if (strcmp(str, def->js_name) == 0) {
      *result = def;
      return true;
    }
    def++;
  }
  return false;
}

JERRY_FUNCTION(prv_set_font) {
  const RockyAPISystemFontDefinition *font_definition = NULL;
  if (argc >= 1 && prv_font_definition_from_value(argv[0], &font_definition)) {
    s_rocky_text_state.font = fonts_get_system_font(font_definition->res_key);
    s_rocky_text_state.font_name = font_definition->js_name;
  }
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_get_font) {
  return jerry_create_string_utf8((const jerry_char_t *)s_rocky_text_state.font_name);
}

JERRY_FUNCTION(prv_measure_text) {
  char *str_buffer;

  ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(
      ROCKY_ARG(str_buffer),
  );

  GContext *const ctx = rocky_api_graphics_get_gcontext();

  const int16_t box_x = argc >= 2 ? (int16_t)jerry_get_int32_value(argv[1]) : 0;
  const int16_t box_y = argc >= 3 ? (int16_t)jerry_get_int32_value(argv[2]) : 0;
  const int16_t box_width = argc >= 4 ? (int16_t)jerry_get_int32_value(argv[3]) : INT16_MAX;
  const GRect box = {
    .origin.x = box_x,
    .origin.y = box_y,
    .size.w = box_width,
    .size.h = INT16_MAX,
  };

  const GSize size = prv_get_max_used_size(ctx, str_buffer, box);
  task_free(str_buffer);

  // return a TextMetrics object
  JS_VAR result = jerry_create_object();
  JS_VAR result_width = jerry_create_number(size.w);
  JS_VAR result_height = jerry_create_number(size.h);
  JS_VAR result_abbl = jerry_create_number(-1);
  JS_VAR result_abbr = jerry_create_number(-2);
  jerry_set_object_field(result, "width", result_width);
  jerry_set_object_field(result, "height", result_height);
  return jerry_acquire_value(result);
}

void rocky_api_graphics_text_add_canvas_methods(jerry_value_t obj) {
  rocky_add_function(obj, ROCKY_CONTEXT2D_FILLTEXT, prv_fill_text);
  rocky_add_function(obj, ROCKY_CONTEXT2D_MEASURETEXT, prv_measure_text);
  rocky_define_property(obj, ROCKY_CONTEXT2D_TEXTALIGN, prv_get_text_align, prv_set_text_align);
  rocky_define_property(obj, ROCKY_CONTEXT2D_FONT, prv_get_font, prv_set_font);
}

static void prv_text_state_deinit(void) {
  if (s_rocky_text_state.text_attributes) {
    graphics_text_attributes_destroy(s_rocky_text_state.text_attributes);
    s_rocky_text_state.text_attributes = NULL;
  }
}

void rocky_api_graphics_text_reset_state(void) {
  prv_text_state_deinit();

  s_rocky_text_state = (RockyAPITextState) {
    .font = s_default_font,
    .font_name = DEFAULT_FONT_DEFINITION.js_name,

    .overflow_mode = GTextOverflowModeWordWrap,
    .alignment = GTextAlignmentLeft,
    .text_attributes = NULL,
  };
}

void rocky_api_graphics_text_init(void) {
  s_default_font = fonts_get_system_font(DEFAULT_FONT_DEFINITION.res_key);
  rocky_api_graphics_text_reset_state();
}

void rocky_api_graphics_text_deinit(void) {
  prv_text_state_deinit();
}
