/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jerry-api.h"
#include "rocky_api_graphics.h"
#include "rocky_api_graphics_path2d.h"
#include "rocky_api_graphics_text.h"
#include "rocky_api_util.h"
#include "rocky_api_util_args.h"

#include "applib/app.h"
#include "applib/app_logging.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/attributes.h"
#include "util/size.h"
#include "util/trig.h"
#include "process_state/app_state/app_state.h"
#include "rocky_api_errors.h"
#include "rocky_api_global.h"
#include "rocky_api_graphics_color.h"

#define ROCKY_EVENT_DRAW "draw"
#define ROCKY_EVENT_DRAW_CONTEXT "context"
#define ROCKY_REQUESTDRAW "requestDraw"
#define ROCKY_CONTEXT2D_CONSTRUCTOR "CanvasRenderingContext2D"
#define ROCKY_CONTEXT2D_CANVAS "canvas"
#define ROCKY_CONTEXT2D_CLEARRECT "clearRect"
#define ROCKY_CONTEXT2D_FILLRECT "fillRect"
#define ROCKY_CONTEXT2D_FILLRADIAL "rockyFillRadial"
#define ROCKY_CONTEXT2D_STROKERECT "strokeRect"
#define ROCKY_CONTEXT2D_LINEWIDTH "lineWidth"
#define ROCKY_CONTEXT2D_STROKESTYLE "strokeStyle"
#define ROCKY_CONTEXT2D_FILLSTYLE "fillStyle"
#define ROCKY_CONTEXT2D_SAVE "save"
#define ROCKY_CONTEXT2D_RESTORE "restore"
#define ROCKY_CANVAS_CONSTRUCTOR "CanvasElement"
#define ROCKY_CANVAS_CLIENTWIDTH "clientWidth"
#define ROCKY_CANVAS_CLIENTHEIGHT "clientHeight"
#define ROCKY_CANVAS_UNOBSTRUCTEDLEFT "unobstructedLeft"
#define ROCKY_CANVAS_UNOBSTRUCTEDTOP "unobstructedTop"
#define ROCKY_CANVAS_UNOBSTRUCTEDWIDTH "unobstructedWidth"
#define ROCKY_CANVAS_UNOBSTRUCTEDHEIGHT "unobstructedHeight"

typedef struct {
  ListNode node;
  GDrawState draw_state;
} Context2DStoredState;

// TODO: PBL-35780 make this part of app_state_get_rocky_runtime_context()
SECTION(".rocky_bss") static Context2DStoredState *s_canvas_context_2d_stored_states;

T_STATIC jerry_value_t prv_create_canvas_context_2d_for_layer(Layer *layer) {
  JS_VAR context_2d = rocky_create_with_constructor(ROCKY_CONTEXT2D_CONSTRUCTOR,
                                                           /* no args: */ NULL, 0);

  JS_VAR canvas = jerry_get_object_field(context_2d, ROCKY_CONTEXT2D_CANVAS);
  {
    JS_VAR client_width = jerry_create_number(layer->bounds.size.w);
    JS_VAR client_height = jerry_create_number(layer->bounds.size.h);
    jerry_set_object_field(canvas, ROCKY_CANVAS_CLIENTWIDTH, client_width);
    jerry_set_object_field(canvas, ROCKY_CANVAS_CLIENTHEIGHT, client_height);
  }

  {
    GRect uo_rect;
    layer_get_unobstructed_bounds(layer, &uo_rect);
    JS_VAR unobstructed_left = jerry_create_number(uo_rect.origin.x);
    JS_VAR unobstructed_right = jerry_create_number(uo_rect.origin.y);
    JS_VAR unobstructed_width = jerry_create_number(uo_rect.size.w);
    JS_VAR unobstructed_height = jerry_create_number(uo_rect.size.h);
    jerry_set_object_field(canvas, ROCKY_CANVAS_UNOBSTRUCTEDLEFT, unobstructed_left);
    jerry_set_object_field(canvas, ROCKY_CANVAS_UNOBSTRUCTEDTOP, unobstructed_right);
    jerry_set_object_field(canvas, ROCKY_CANVAS_UNOBSTRUCTEDWIDTH, unobstructed_width);
    jerry_set_object_field(canvas, ROCKY_CANVAS_UNOBSTRUCTEDHEIGHT, unobstructed_height);
  }

  return jerry_acquire_value(context_2d);
}

static void prv_rocky_update_proc(Layer *layer, GContext *ctx) {
  if (!rocky_global_has_event_handlers(ROCKY_EVENT_DRAW)) {
    return;
  }
  rocky_api_graphics_text_reset_state();
  rocky_api_graphics_path2d_reset_state();
  JS_VAR event = rocky_global_create_event(ROCKY_EVENT_DRAW);
  JS_VAR context_2d = prv_create_canvas_context_2d_for_layer(layer);
  jerry_set_object_field(event, ROCKY_EVENT_DRAW_CONTEXT, context_2d);
  rocky_global_call_event_handlers(event);
  rocky_api_graphics_path2d_reset_state();
}

JERRY_FUNCTION(prv_request_draw) {
  Window *const top_window = app_window_stack_get_top_window();
  if (!top_window) {
    return jerry_create_undefined();
  }

  layer_mark_dirty(&top_window->layer);
  return jerry_create_undefined();
}

GContext *rocky_api_graphics_get_gcontext(void) {
  return app_state_get_graphics_context();
}

static jerry_value_t prv_rect_precise_call(const jerry_length_t argc, const jerry_value_t argv[],
                                   void (*func)(GContext *, const GRectPrecise *)) {
  GRectPrecise rect;
  ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(ROCKY_ARG(rect));

  GContext *const ctx = rocky_api_graphics_get_gcontext();
  func(ctx, &rect);
  return jerry_create_undefined();
}

static GRect prv_grect_from_precise(const GRectPrecise *rect) {
  const int16_t x = Fixed_S16_3_rounded_int(rect->origin.x);
  const int16_t y = Fixed_S16_3_rounded_int(rect->origin.y);
  const int16_t w = Fixed_S16_3_rounded_int(grect_precise_get_max_x(rect)) - x;
  const int16_t h = Fixed_S16_3_rounded_int(grect_precise_get_max_y(rect)) - y;

  return GRect(x, y, w, h);
}

static jerry_value_t prv_rect_call(const jerry_length_t argc, const jerry_value_t argv[],
                                   void (*func)(GContext *, const GRect *)) {
  GRectPrecise prect;
  ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(
    ROCKY_ARG(prect),
  );

  GContext *const ctx = rocky_api_graphics_get_gcontext();
  const GRect rect = prv_grect_from_precise(&prect);
  func(ctx, &rect);
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_fill_rect) {
  return prv_rect_call(argc, argv, graphics_fill_rect);
}

static void prv_draw_rect_impl(GContext *ctx, const GRectPrecise *rect) {
  GRectPrecise adjusted_rect = *rect;
  adjusted_rect.origin.x.raw_value -= FIXED_S16_3_HALF.raw_value;
  adjusted_rect.origin.y.raw_value -= FIXED_S16_3_HALF.raw_value;
  graphics_draw_rect_precise(ctx, &adjusted_rect);
}

JERRY_FUNCTION(prv_stroke_rect) {
  return prv_rect_precise_call(argc, argv, prv_draw_rect_impl);
}

JERRY_FUNCTION(prv_clear_rect) {
  GContext *const ctx = rocky_api_graphics_get_gcontext();
  const GColor prev_color = ctx->draw_state.fill_color;
  ctx->draw_state.fill_color = GColorBlack;
  JS_VAR result = prv_rect_call(argc, argv, graphics_fill_rect);
  ctx->draw_state.fill_color = prev_color;
  return jerry_acquire_value(result);
}

JERRY_FUNCTION(prv_set_stroke_width) {
  uint8_t width;
  ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(
      ROCKY_ARG(width),
  );
  graphics_context_set_stroke_width(rocky_api_graphics_get_gcontext(), width);
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_get_stroke_width) {
  return jerry_create_number((double)rocky_api_graphics_get_gcontext()->draw_state.stroke_width);
}

static jerry_value_t prv_graphics_set_color(const jerry_length_t argc,
                                            const jerry_value_t argv[],
                                            void (*func)(GContext *, GColor)) {
  GColor color;
  const RockyArgBinding binding = ROCKY_ARG(color);
  JS_VAR error_value = rocky_args_assign(argc, argv, &binding, 1);
  // Canvas APIs do a no-op if the color string is invalid
  if (!jerry_value_has_error_flag(error_value)) {
    func(rocky_api_graphics_get_gcontext(), color);
  };

  return jerry_create_undefined();
}

#define COLOR_BUFFER_LENGTH (12)
T_STATIC void prv_graphics_color_to_char_buffer(GColor8 color, char *buf_out) {
  if (color.a <= 1) {
    strncpy(buf_out, "transparent", COLOR_BUFFER_LENGTH);
  } else {
    snprintf(buf_out, COLOR_BUFFER_LENGTH, "#%02X%02X%02X",
             color.r * 85, color.g * 85, color.b * 85);
  }
}

static jerry_value_t prv_graphics_get_color_string(GColor8 color) {
  char buf[COLOR_BUFFER_LENGTH];
  prv_graphics_color_to_char_buffer(color, buf);
  return jerry_create_string((const jerry_char_t *)buf);
}

JERRY_FUNCTION(prv_set_stroke_style) {
  return prv_graphics_set_color(argc, argv, graphics_context_set_stroke_color);
}

JERRY_FUNCTION(prv_get_stroke_style) {
  return prv_graphics_get_color_string(rocky_api_graphics_get_gcontext()->draw_state.stroke_color);
}

JERRY_FUNCTION(prv_set_fill_style) {
  return prv_graphics_set_color(argc, argv, graphics_context_set_fill_color);
}

JERRY_FUNCTION(prv_get_fill_style) {
  return prv_graphics_get_color_string(rocky_api_graphics_get_gcontext()->draw_state.fill_color);
}

JERRY_FUNCTION(prv_fill_radial) {
  // pblFillRadial(cx, cy, radius1, radius2, angle1, angle2)

  // TODO: PBL-40555 consolidate angle handling here and in rocky_api_path2d.c
  GPointPrecise center;
  Fixed_S16_3 radius1, radius2;
  double angle_1, angle_2;
  ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(
      ROCKY_ARG(center.x),
      ROCKY_ARG(center.y),
      ROCKY_ARG(radius1),
      ROCKY_ARG(radius2),
      ROCKY_ARG_ANGLE(angle_1),
      ROCKY_ARG_ANGLE(angle_2),
  );

  // adjust for coordinate system
  center.x.raw_value -= FIXED_S16_3_HALF.raw_value;
  center.y.raw_value -= FIXED_S16_3_HALF.raw_value;

  radius1.raw_value = MAX(0, radius1.raw_value);
  radius2.raw_value = MAX(0, radius2.raw_value);
  const Fixed_S16_3 inner_radius = Fixed_S16_3(MIN(radius1.raw_value, radius2.raw_value));
  const Fixed_S16_3 outer_radius = Fixed_S16_3(MAX(radius1.raw_value, radius2.raw_value));

  GContext *const ctx = rocky_api_graphics_get_gcontext();
  graphics_fill_radial_precise_internal(ctx, center, inner_radius, outer_radius,
                                        (int32_t)angle_1, (int32_t)angle_2);
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_save) {
  Context2DStoredState *const new_head = task_zalloc(sizeof(*new_head));
  new_head->draw_state = rocky_api_graphics_get_gcontext()->draw_state;
  list_insert_before(&s_canvas_context_2d_stored_states->node, &new_head->node);
  s_canvas_context_2d_stored_states = new_head;
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_restore) {
  Context2DStoredState *const head = s_canvas_context_2d_stored_states;
  if (head) {
    rocky_api_graphics_get_gcontext()->draw_state = head->draw_state;
    list_remove(&head->node, (ListNode **)&s_canvas_context_2d_stored_states, NULL);
    task_free(head);
  }

  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_canvas_rendering_context_2d_constructor) {
  JS_VAR canvas = rocky_create_with_constructor(ROCKY_CANVAS_CONSTRUCTOR,
                                                       /* no args: */ NULL, 0);
  jerry_set_object_field(this_val, ROCKY_CONTEXT2D_CANVAS, canvas);
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_canvas_constructor) {
  return jerry_create_undefined();
}

static void prv_configure_top_window_and_create_constructors(void) {
  Window *const window = app_window_stack_get_top_window();
  // rocky graphics require a window to already be on the current window stack
  PBL_ASSERTN(window);
  window->layer.update_proc = prv_rocky_update_proc;

  // Create the CanvasRenderingContext2D constructor:
  JS_VAR ctx_prototype =
      rocky_add_constructor(ROCKY_CONTEXT2D_CONSTRUCTOR,
                            prv_canvas_rendering_context_2d_constructor);

  jerry_set_object_field(ctx_prototype, ROCKY_CONTEXT2D_CANVAS, jerry_create_undefined());
  rocky_add_function(ctx_prototype, ROCKY_CONTEXT2D_CLEARRECT, prv_clear_rect);
  rocky_add_function(ctx_prototype, ROCKY_CONTEXT2D_FILLRECT, prv_fill_rect);
  rocky_add_function(ctx_prototype, ROCKY_CONTEXT2D_FILLRADIAL, prv_fill_radial);
  rocky_add_function(ctx_prototype, ROCKY_CONTEXT2D_STROKERECT, prv_stroke_rect);
  rocky_add_function(ctx_prototype, ROCKY_CONTEXT2D_SAVE, prv_save);
  rocky_add_function(ctx_prototype, ROCKY_CONTEXT2D_RESTORE, prv_restore);
  rocky_define_property(ctx_prototype, ROCKY_CONTEXT2D_LINEWIDTH,
                        prv_get_stroke_width, prv_set_stroke_width);
  rocky_define_property(ctx_prototype, ROCKY_CONTEXT2D_STROKESTYLE,
                        prv_get_stroke_style, prv_set_stroke_style);
  rocky_define_property(ctx_prototype, ROCKY_CONTEXT2D_FILLSTYLE,
                        prv_get_fill_style, prv_set_fill_style);

  rocky_api_graphics_path2d_add_canvas_methods(ctx_prototype);
  rocky_api_graphics_text_add_canvas_methods(ctx_prototype);

  // Create the CanvasElement constructor:
  JS_UNUSED_VAL = rocky_add_constructor(ROCKY_CANVAS_CONSTRUCTOR, prv_canvas_constructor);
}

static void prv_init_apis(void) {
  prv_configure_top_window_and_create_constructors();
  JS_VAR rocky = rocky_get_rocky_singleton();
  rocky_add_function(rocky, ROCKY_REQUESTDRAW, prv_request_draw);
  rocky_api_graphics_text_init();
  rocky_api_graphics_path2d_reset_state(); // does not have an init, so we call reset_state()
}

static void prv_deinit_apis(void) {
  while (s_canvas_context_2d_stored_states) {
    Context2DStoredState *state = s_canvas_context_2d_stored_states;
    s_canvas_context_2d_stored_states =
      (Context2DStoredState *)s_canvas_context_2d_stored_states->node.next;
    task_free(state);
  }
  rocky_api_graphics_text_deinit();
  rocky_api_graphics_path2d_reset_state();
}

static bool prv_add_handler(const char *event_name, jerry_value_t handler) {
  return strcmp(ROCKY_EVENT_DRAW, event_name) == 0;
}

const RockyGlobalAPI GRAPHIC_APIS = {
  .init = prv_init_apis,
  .deinit = prv_deinit_apis,
  .add_handler = prv_add_handler,
};
