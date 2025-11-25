/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "transform.h"
#include "scale_segmented.h"

#include "applib/applib_malloc.auto.h"
#include "applib/graphics/gdraw_command_transforms.h"
#include "applib/ui/animation.h"
#include "applib/ui/animation_interpolate.h"
#include "applib/ui/animation_timing.h"
#include "applib/ui/kino/kino_reel.h"
#include "system/logging.h"

typedef struct {
  GPoint bounce;
  GPointIndexLookup *index_lookup;
  InterpolateInt64Function interpolate;

  Fixed_S32_16 point_duration;
  Fixed_S32_16 effect_duration;
  int16_t expand;

  struct {
    AnimationCurveFunction curve;

    Fixed_S16_3 from;
    Fixed_S16_3 to;

    GStrokeWidthOp from_op;
    GStrokeWidthOp to_op;
  } stroke_width;

  struct {
    GPointIndexLookupCreator creator;
    void *userdata;
    bool owns_userdata;
  } lookup;
} ScaleSegmentedData;


typedef struct {
  GPoint target;
} DistanceLookupData;

static GPointIndexLookup *prv_create_lookup_by_distance(GDelayCreatorContext *ctx, void *userdata) {
  ctx->owns_lookup = true;
  DistanceLookupData *data = userdata;
  return gdraw_command_list_create_index_lookup_by_distance(ctx->list, data->target);
};

static void prv_destructor(void *context) {
  ScaleSegmentedData *data = context;
  if (data->lookup.owns_userdata) {
    applib_free(data->lookup.userdata);
  }
  applib_free(context);
}

static void prv_apply_transform(GDrawCommandList *list, GSize size, const GRect *from,
                                const GRect *to, AnimationProgress normalized, void *context) {
  if (!list || !context) {
    return;
  }
  ScaleSegmentedData *data = context;

  GDelayCreatorContext delay_ctx = {
    .list = list,
    .size = size,
  };
  if (!data->lookup.creator) {
    return;
  }
  GPointIndexLookup *index_lookup = data->lookup.creator(&delay_ctx, data->lookup.userdata);

  GRect intermediate;
  AnimationProgress second_normalized;

  const bool two_stage = (data->expand || data->bounce.x || data->bounce.y);

  if (two_stage) {
    intermediate = grect_scalar_expand(*to, data->expand);
    gpoint_add_eq(&intermediate.origin, data->bounce);

    const AnimationProgress first_normalized = animation_timing_segmented(
        normalized, 0, 2, data->effect_duration);
    gdraw_command_list_scale_segmented_to(
        list, size, *from, intermediate, first_normalized, data->interpolate, index_lookup,
        data->point_duration, false);

    size = intermediate.size;
    second_normalized = animation_timing_segmented(normalized, 1, 2, data->effect_duration);
  } else {
    intermediate = *from;
    second_normalized = normalized;
  }

  gdraw_command_list_scale_segmented_to(
      list, size, intermediate, *to, second_normalized, data->interpolate, index_lookup,
      data->point_duration, two_stage);

  const AnimationProgress stroke_width_progress = data->stroke_width.curve ?
      data->stroke_width.curve(normalized) :
      animation_timing_curve(normalized, AnimationCurveEaseInOut);
  gdraw_command_list_scale_stroke_width(
      list, data->stroke_width.from, data->stroke_width.to,
      data->stroke_width.from_op, data->stroke_width.to_op, stroke_width_progress);

  if (delay_ctx.owns_lookup) {
    applib_free(index_lookup);
  }
}

static GPoint prv_calc_bounce_offset(GRect from, GRect to, int16_t bounce) {
  GPoint bounce_offset = GPointZero;

  const int16_t delta_x = to.origin.x - from.origin.x;
  const int16_t delta_y = to.origin.y - from.origin.y;

  if (!delta_x && !delta_y) {
    return bounce_offset;
  }

  const int16_t magnitude = integer_sqrt(delta_x * delta_x + delta_y * delta_y);
  if (!magnitude) {
    return bounce_offset;
  }

  bounce_offset.x = bounce * delta_x / magnitude;
  bounce_offset.y = bounce * delta_y / magnitude;
  return bounce_offset;
}

static const TransformImpl SCALE_SEGMENTED_TRANSFORM_IMPL = {
  .destructor = prv_destructor,
  .apply = prv_apply_transform,
};

KinoReel *kino_reel_scale_segmented_create(KinoReel *from_reel, bool take_ownership,
                                           GRect screen_frame) {
  ScaleSegmentedData *data = applib_malloc(sizeof(ScaleSegmentedData));
  if (!data) {
    return NULL;
  }

  *data = (ScaleSegmentedData) {
    .point_duration = SCALE_SEGMENTED_DEFAULT_POINT_DURATION,
    .effect_duration = SCALE_SEGMENTED_DEFAULT_EFFECT_DURATION,
    .stroke_width = {
      .from = FIXED_S16_3_ONE,
      .to = FIXED_S16_3_ONE,
      .from_op = GStrokeWidthOpMultiply,
      .to_op = GStrokeWidthOpMultiply,
    },
  };

  KinoReel *reel = kino_reel_transform_create(&SCALE_SEGMENTED_TRANSFORM_IMPL, data);
  if (reel) {
    kino_reel_transform_set_from_reel(reel, from_reel, take_ownership);
    kino_reel_transform_set_layer_frame(reel, screen_frame);
    kino_reel_transform_set_from_frame(reel, screen_frame);
    kino_reel_transform_set_to_frame(reel, screen_frame);
    kino_reel_transform_set_global(reel, true);
  } else {
    prv_destructor(data);
  }
  return reel;
}

void kino_reel_scale_segmented_set_delay_lookup_creator(
    KinoReel *reel, GPointIndexLookupCreator creator, void *userdata, bool take_ownership) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (!data) {
    return;
  }
  if (data->lookup.owns_userdata) {
    applib_free(data->lookup.userdata);
  }
  data->lookup.creator = creator;
  data->lookup.userdata = userdata;
  data->lookup.owns_userdata = take_ownership;
}

bool kino_reel_scale_segmented_set_delay_by_distance(KinoReel *reel, GPoint target) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (!data) {
    return false;
  }
  KinoReel *from_reel = kino_reel_transform_get_from_reel(reel);
  GDrawCommandList *list = kino_reel_get_gdraw_command_list(from_reel);
  if (!list) {
    return false;
  }
  DistanceLookupData *lookup_data = applib_malloc(sizeof(DistanceLookupData));
  if (!lookup_data) {
    return false;
  }
  *lookup_data = (DistanceLookupData) { .target = target };
  const bool take_ownership = true;
  kino_reel_scale_segmented_set_delay_lookup_creator(reel, prv_create_lookup_by_distance,
                                                     lookup_data, take_ownership);
  return true;
}

void kino_reel_scale_segmented_set_point_duration(KinoReel *reel, Fixed_S32_16 point_duration) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    data->point_duration = point_duration;
  }
}

void kino_reel_scale_segmented_set_effect_duration(KinoReel *reel, Fixed_S32_16 effect_duration) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    data->effect_duration = effect_duration;
  }
}

void kino_reel_scale_segmented_set_interpolate(KinoReel *reel,
                                               InterpolateInt64Function interpolate) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    data->interpolate = interpolate;
  }
}

void kino_reel_scale_segmented_set_deflate_effect(KinoReel *reel, int16_t expand) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    data->expand = expand;
  }
}

void kino_reel_scale_segmented_set_bounce_effect(KinoReel *reel, int16_t bounce) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    GRect from = kino_reel_transform_get_from_frame(reel);
    GRect to = kino_reel_transform_get_to_frame(reel);
    data->bounce = bounce ? prv_calc_bounce_offset(from, to, bounce) : GPointZero;
  }
}

void kino_reel_scale_segmented_set_from_stroke_width(KinoReel *reel, Fixed_S16_3 from,
                                                     GStrokeWidthOp from_op) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    data->stroke_width.from = from;
    data->stroke_width.from_op = from_op;
  }
}

void kino_reel_scale_segmented_set_to_stroke_width(KinoReel *reel, Fixed_S16_3 to,
                                                   GStrokeWidthOp to_op) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    data->stroke_width.to = to;
    data->stroke_width.to_op = to_op;
  }
}

void kino_reel_scale_segmented_set_stroke_width_curve(KinoReel *reel,
                                                      AnimationCurveFunction curve) {
  ScaleSegmentedData *data = kino_reel_transform_get_context(reel);
  if (data) {
    data->stroke_width.curve = curve;
  }
}

static AnimationProgress prv_ease_in_out_last_half(AnimationProgress progress) {
  return animation_timing_curve(animation_timing_clip(2 * (progress - ANIMATION_NORMALIZED_MAX / 2))
      , AnimationCurveEaseInOut);
}

void kino_reel_scale_segmented_set_end_as_dot(KinoReel *reel, int16_t radius) {
  GRect frame = kino_reel_transform_get_to_frame(reel);
  kino_reel_transform_set_to_frame(
      reel, (GRect) { grect_center_point(&frame), SCALE_SEGMENTED_DOT_SIZE });

  const Fixed_S16_3 to = Fixed_S16_3((2 * radius) << FIXED_S16_3_PRECISION);
  kino_reel_scale_segmented_set_to_stroke_width(reel, to, GStrokeWidthOpSet);
  kino_reel_scale_segmented_set_stroke_width_curve(reel, prv_ease_in_out_last_half);
}
