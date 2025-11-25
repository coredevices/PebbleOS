/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_graphics_path2d.h"
#include "rocky_api_graphics.h"
#include "rocky_api_errors.h"

#include "applib/graphics/gpath.h"
#include "applib/graphics/graphics_circle.h"
#include "applib/graphics/graphics_line.h"
#include "kernel/pbl_malloc.h"
#include "rocky_api_util.h"
#include "rocky_api_util_args.h"
#include "system/passert.h"
#include "util/size.h"
#include "util/trig.h"

#define PATH2D_ARC "arc"
#define PATH2D_RECT "rect"
#define PATH2D_BEGINPATH "beginPath"
#define PATH2D_MOVETO "moveTo"
#define PATH2D_LINETO "lineTo"
#define PATH2D_CLOSEPATH "closePath"
#define ROCKY_CONTEXT2D_STROKE "stroke"
#define ROCKY_CONTEXT2D_FILL "fill"

#define MINIMUM_ARRAY_LEN (8)

// TODO: PBL-35780 make this part of app_state_get_rocky_runtime_context()
SECTION(".rocky_bss") T_STATIC RockyAPIPathStep *s_rocky_path_steps;
SECTION(".rocky_bss") T_STATIC size_t s_rocky_path_steps_array_len;
SECTION(".rocky_bss") T_STATIC size_t s_rocky_path_steps_num;

void rocky_api_graphics_path2d_reset_state(void) {
  s_rocky_path_steps_num = 0;

  task_free(s_rocky_path_steps);
  s_rocky_path_steps = NULL;
  s_rocky_path_steps_array_len = 0;
}

JERRY_FUNCTION(prv_begin_path) {
  rocky_api_graphics_path2d_reset_state();
  return jerry_create_undefined();
}

static size_t prv_get_realloc_array_len(const size_t required_array_len) {
  size_t len = s_rocky_path_steps_array_len ?: MINIMUM_ARRAY_LEN;
  while (required_array_len > len) {
    len *= 2;
  }
  return len;
}

static jerry_value_t prv_try_allocate_steps(const size_t num_steps_increment) {
  const size_t required_array_len = (s_rocky_path_steps_num + num_steps_increment);
  if (required_array_len <= s_rocky_path_steps_array_len) {
    goto success;
  }
  const size_t new_array_len = prv_get_realloc_array_len(required_array_len);
  void *new_steps_array = task_realloc(s_rocky_path_steps,
                                       sizeof(RockyAPIPathStep) * new_array_len);
  if (!new_steps_array) {
    return rocky_error_oom("can't create more path steps");
  }
  s_rocky_path_steps = new_steps_array;
  s_rocky_path_steps_array_len = new_array_len;
success:
  return jerry_create_undefined();
}

#define TRY_ALLOCATE_STEPS_OR_RETURN_ERROR(num_steps_increment) \
  ROCKY_RETURN_IF_ERROR(prv_try_allocate_steps(num_steps_increment))

static jerry_value_t prv_add_pt(jerry_length_t argc, const jerry_value_t *argv,
                                RockyAPIPathStepType step_type) {
  const double raw_x = argc > 0 ? (jerry_get_number_value(argv[0]) - 0.5) * FIXED_S16_3_FACTOR : 0;
  const double raw_y = argc > 1 ? (jerry_get_number_value(argv[1]) - 0.5) * FIXED_S16_3_FACTOR : 0;

  if (raw_x < INT16_MIN || raw_x > INT16_MAX || raw_y < INT16_MIN || raw_x > INT16_MAX) {
    return rocky_error_argument_invalid("Value out of bounds");
  }

  TRY_ALLOCATE_STEPS_OR_RETURN_ERROR(1);

  s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
    .type = step_type,
    .pt.xy = GPointPrecise((int16_t)raw_x, (int16_t)raw_y),
  };

  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_move_to) {
  return prv_add_pt(argc, argv, RockyAPIPathStepType_MoveTo);
}

JERRY_FUNCTION(prv_line_to) {
  return prv_add_pt(argc, argv, RockyAPIPathStepType_LineTo);
}

JERRY_FUNCTION(prv_stroke) {
  GContext *const ctx = rocky_api_graphics_get_gcontext();

  GPoint p = {0};
  GPointPrecise pp = {.x = {0}, .y = {0}};
  bool moved_already = false;

  #define ASSIGN_P(new_p) do { \
    moved_already = true; \
    p = GPointFromGPointPrecise(new_p); \
    pp = new_p; \
  } while (0)

  for (size_t i = 0; i < s_rocky_path_steps_num; i++) {
    RockyAPIPathStep *const step = &s_rocky_path_steps[i];

    switch (step->type) {
      case RockyAPIPathStepType_MoveTo: {
        ASSIGN_P(step->pt.xy);
        break;
      }
      case RockyAPIPathStepType_LineTo: {
        if (moved_already) {
          graphics_line_draw_precise_stroked(ctx, pp, step->pt.xy);
        }
        ASSIGN_P(step->pt.xy);
        break;
      }
      case RockyAPIPathStepType_Arc: {
        if (moved_already) {
          const GPointPrecise pt_from = gpoint_from_polar_precise(
            &step->arc.center, (uint16_t)step->arc.radius.raw_value, step->arc.angle_start);
          graphics_line_draw_precise_stroked(ctx, pp, pt_from);
        }

        int32_t angle_start = step->arc.angle_start;
        int32_t angle_end = step->arc.angle_end;
        if (step->arc.anti_clockwise) {
          const int32_t t = angle_start;
          angle_start = angle_end;
          angle_end = t;
        }
        while (angle_end < angle_start) {
          angle_end += TRIG_MAX_ANGLE;
        }
        graphics_draw_arc_precise_internal(ctx, step->arc.center, step->arc.radius,
                                           angle_start, angle_end);

        const GPointPrecise pt_to = gpoint_from_polar_precise(
          &step->arc.center, (uint16_t)step->arc.radius.raw_value, step->arc.angle_end);
        ASSIGN_P(pt_to);
        break;
      }
      default:
        WTF;
    }
  }
  #undef ASSIGN_P
  return jerry_create_undefined();
}

static void prv_fill_points(GPoint *points, size_t num) {
  if (num < 3) {
    return;
  }

  GPath path = {
    .num_points = num,
    .points = points,
  };
  GContext *const ctx = rocky_api_graphics_get_gcontext();
  gpath_draw_filled(ctx, &path);
}

static GPointPrecise prv_point_add_vector_precise(GPointPrecise *pt, GVectorPrecise *v) {
  return GPointPrecise(
    pt->x.raw_value + v->dx.raw_value,
    pt->y.raw_value + v->dy.raw_value);
}

JERRY_FUNCTION(prv_fill) {
  GPoint *const points = task_zalloc(sizeof(*points) * s_rocky_path_steps_num);
  if (!points) {
    return rocky_error_oom("too many points to fill");
  }
  size_t points_num = 0;

  jerry_value_t rv;

  #define ADD_P(pt) \
    PBL_ASSERTN(points_num < s_rocky_path_steps_num); \
    points[points_num++] = GPointFromGPointPrecise( \
      prv_point_add_vector_precise(&(pt).xy, &(pt).fill_delta));

  for (size_t i = 0; i < s_rocky_path_steps_num; i++) {
    RockyAPIPathStep *const step = &s_rocky_path_steps[i];
    switch (step->type) {
      case RockyAPIPathStepType_MoveTo: {
        prv_fill_points(points, points_num);
        points_num = 0;
        ADD_P(step->pt);
        break;
      }
      case RockyAPIPathStepType_LineTo: {
        ADD_P(step->pt);
        break;
      }
      case RockyAPIPathStepType_Arc: {
        rv = rocky_error_argument_invalid("fill() does not support arc()");
        goto cleanup;
      }
    }
  }
  rv = jerry_create_undefined();

  prv_fill_points(points, points_num);

cleanup:
  task_free(points);
  return rv;
}

JERRY_FUNCTION(prv_arc) {
  GPointPrecise center;
  Fixed_S16_3 radius;
  double angle_1, angle_2;
  ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(
    ROCKY_ARG(center.x),
    ROCKY_ARG(center.y),
    ROCKY_ARG(radius),
    ROCKY_ARG_ANGLE(angle_1),
    ROCKY_ARG_ANGLE(angle_2));

  TRY_ALLOCATE_STEPS_OR_RETURN_ERROR(1);

  const bool anti_clockwise = (argc >= 6) ? jerry_get_boolean_value(argv[5]) : false;

  // adjust for coordinate system
  center.x.raw_value -= FIXED_S16_3_HALF.raw_value;
  center.y.raw_value -= FIXED_S16_3_HALF.raw_value;

  s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
    .type = RockyAPIPathStepType_Arc,
    .arc = (RockyAPIPathStepArc) {
      .center = center,
      .radius = radius,
      // TODO: PBL-40555 consolidate angle handling here and in rocky_api_graphics.c
      .angle_start = jerry_get_angle_value(argv[3]),
      .angle_end = jerry_get_angle_value(argv[4]),
      .anti_clockwise = anti_clockwise,
    },
  };

  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_rect) {
  TRY_ALLOCATE_STEPS_OR_RETURN_ERROR(5);

  if (argc >= 4) {
    GRectPrecise rect;
    ROCKY_ARGS_ASSIGN_OR_RETURN_ERROR(ROCKY_ARG(rect));
    grect_precise_standardize(&rect);

    // shift rectangle into coordinate system
    const int16_t half_pt = FIXED_S16_3_HALF.raw_value;
    rect.origin.x.raw_value -= half_pt;
    rect.origin.y.raw_value -= half_pt;

    // special casing for our filling algorithm to match fillRect()
    const int16_t full_pt = FIXED_S16_3_ONE.raw_value;
    const int16_t delta_t = full_pt;
    const int16_t delta_r = full_pt;
    const int16_t delta_b = 0;
    const int16_t delta_l = 0;
    const GVectorPrecise delta_tl = GVectorPrecise(delta_l, delta_t);
    const GVectorPrecise delta_tr = GVectorPrecise(delta_r, delta_t);
    const GVectorPrecise delta_br = GVectorPrecise(delta_r, delta_b);
    const GVectorPrecise delta_bl = GVectorPrecise(delta_l, delta_b);

    const Fixed_S16_3 right = grect_precise_get_max_x(&rect);
    const Fixed_S16_3 bottom = grect_precise_get_max_y(&rect);

    // top left
    s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
      .type = RockyAPIPathStepType_MoveTo,
      .pt.xy = rect.origin,
      .pt.fill_delta = delta_tl,
    };
    // top right
    s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
      .type = RockyAPIPathStepType_LineTo,
      .pt.xy = GPointPrecise(right.raw_value, rect.origin.y.raw_value),
      .pt.fill_delta = delta_tr,
    };
    // bottom right
    s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
      .type = RockyAPIPathStepType_LineTo,
      .pt.xy = GPointPrecise(right.raw_value, bottom.raw_value),
      .pt.fill_delta = delta_br,
    };
    // bottom left
    s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
      .type = RockyAPIPathStepType_LineTo,
      .pt.xy = GPointPrecise(rect.origin.x.raw_value, bottom.raw_value),
      .pt.fill_delta = delta_bl,
    };
    // top left again, to close path
    s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
      .type = RockyAPIPathStepType_LineTo,
      .pt.xy = rect.origin,
      .pt.fill_delta = delta_tl,
    };
  }
  return jerry_create_undefined();
}

JERRY_FUNCTION(prv_close_path) {
  // lineTo() back to most-recent .moveTo()

  if (s_rocky_path_steps_num < 2) {
    return jerry_create_undefined();
  }

  RockyAPIPathStep *step = &s_rocky_path_steps[s_rocky_path_steps_num - 1];

  // if the last step was a moveTo(), there's nothing to do
  if (step->type == RockyAPIPathStepType_MoveTo) {
    return jerry_create_undefined();
  }

  TRY_ALLOCATE_STEPS_OR_RETURN_ERROR(1);

  do {
    step--;
    if (step->type == RockyAPIPathStepType_MoveTo) {
      // add a lintTo() at the end
      s_rocky_path_steps[s_rocky_path_steps_num++] = (RockyAPIPathStep) {
        .type = RockyAPIPathStepType_LineTo,
        .pt = step->pt,
      };
      break;
    }
  } while (step > &s_rocky_path_steps[0]);

  return jerry_create_undefined();
}

void rocky_api_graphics_path2d_add_canvas_methods(jerry_value_t obj) {
  rocky_add_function(obj, PATH2D_BEGINPATH, prv_begin_path);
  rocky_add_function(obj, PATH2D_MOVETO, prv_move_to);
  rocky_add_function(obj, PATH2D_LINETO, prv_line_to);
  rocky_add_function(obj, PATH2D_ARC, prv_arc);
  rocky_add_function(obj, PATH2D_RECT, prv_rect);
  rocky_add_function(obj, PATH2D_CLOSEPATH, prv_close_path);
  rocky_add_function(obj, ROCKY_CONTEXT2D_STROKE, prv_stroke);
  rocky_add_function(obj, ROCKY_CONTEXT2D_FILL, prv_fill);
}

//! For unit testing
jerry_value_t rocky_api_graphics_path2d_try_allocate_steps(size_t inc_steps) {
  TRY_ALLOCATE_STEPS_OR_RETURN_ERROR(inc_steps);
  return jerry_create_undefined();
}

size_t rocky_api_graphics_path2d_min_array_len(void) {
  return MINIMUM_ARRAY_LEN;
}

size_t rocky_api_graphics_path2d_array_len(void) {
  return s_rocky_path_steps_array_len;
}

jerry_value_t rocky_api_graphics_path2d_call_fill(void) {
  // Args aren't correct, but it doesn't matter right now because the function doesn't use them:
  return prv_fill(jerry_create_undefined(), jerry_create_undefined(), NULL, 0);
}
