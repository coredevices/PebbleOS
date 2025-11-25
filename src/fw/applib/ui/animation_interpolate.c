/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "animation.h"
#include "animation_interpolate.h"
#include "animation_private.h"

#include "applib/graphics/gtypes.h"
#include "system/passert.h"
#include "util/math.h"
#include "util/size.h"

int64_t interpolate_int64_linear(int32_t normalized, int64_t from, int64_t to) {
  return from + ((normalized * (to - from)) / ANIMATION_NORMALIZED_MAX);
}

int64_t interpolate_int64(int32_t normalized, int64_t from, int64_t to) {
  InterpolateInt64Function interpolate =
      animation_private_current_interpolate_override() ?: interpolate_int64_linear;
  return interpolate(normalized, from, to);
}

int16_t interpolate_int16(int32_t normalized, int16_t from, int16_t to) {
  const int64_t interpolated = interpolate_int64(normalized, from, to);
  return (int16_t)CLIP(interpolated, INT16_MIN, INT16_MAX);
}

uint32_t interpolate_uint32(int32_t normalized, uint32_t from, uint32_t to) {
  const int64_t interpolated = interpolate_int64(normalized, from, to);
  return (uint32_t)CLIP(interpolated, 0, UINT32_MAX);
}

Fixed_S32_16 interpolate_fixed32(int32_t normalized, Fixed_S32_16 from, Fixed_S32_16 to) {
  const int64_t interpolated = interpolate_int64(normalized, from.raw_value, to.raw_value);
  const int32_t raw_value =
      (int32_t) CLIP(interpolated, INT32_MIN, INT32_MAX);
  return Fixed_S32_16(raw_value);
}

GSize interpolate_gsize(int32_t normalized, GSize from, GSize to) {
  return (GSize) {
    .w = interpolate_int16(normalized, from.w, to.w),
    .h = interpolate_int16(normalized, from.w, to.w),
  };
}

GPoint interpolate_gpoint(int32_t normalized, GPoint from, GPoint to) {
  return (GPoint) {
    .x = interpolate_int16(normalized, from.x, to.x),
    .y = interpolate_int16(normalized, from.y, to.y),
  };
}

int16_t scale_int16(int16_t value, int16_t from, int16_t to) {
  return (int16_t) ((int32_t) value * to / from);
}

int32_t scale_int32(int32_t value, int32_t from, int32_t to) {
  return (int32_t) ((int64_t) value * to / from);
}

// -------------------------------------------------------

// these values are directly taken from "easing red line 001.mov"
// _in will be added to first value (easing in, anticipation)
// _out will be added to second value (overshoot, swing-back)
static const int32_t s_delta_moook_in[] = {0, 1, 20};
static const int32_t s_delta_moook_out[] = {INTERPOLATE_MOOOK_BOUNCE_BACK, 2, 1, 0};

// TODO: export these as interpolation functions as well
uint32_t interpolate_moook_in_duration() {
  return ARRAY_LENGTH(s_delta_moook_in) * ANIMATION_TARGET_FRAME_INTERVAL_MS;
}

uint32_t interpolate_moook_out_duration() {
  return ARRAY_LENGTH(s_delta_moook_out) * ANIMATION_TARGET_FRAME_INTERVAL_MS;
}

uint32_t interpolate_moook_duration() {
  return interpolate_moook_in_duration() + interpolate_moook_out_duration();
}

uint32_t interpolate_moook_soft_duration(int32_t num_frames_mid) {
  return interpolate_moook_duration() + num_frames_mid * ANIMATION_TARGET_FRAME_INTERVAL_MS;
}

uint32_t interpolate_moook_custom_duration(const MoookConfig *config) {
  PBL_ASSERTN(config);
  return ((config->num_frames_in + config->num_frames_mid + config->num_frames_out) *
          ANIMATION_TARGET_FRAME_INTERVAL_MS);
}

static int64_t prv_interpolate_moook(
    int32_t normalized, int64_t from, int64_t to, const int32_t *frames_in, int32_t num_frames_in,
    const int32_t *frames_out, int32_t num_frames_out, int32_t num_frames_mid, bool bounce_back) {
  const int32_t direction = ((from == to) ? 0 : ((from < to) ? 1 : -1));
  if (direction == 0) {
    return from;
  }

  const int32_t direction_out = direction * (bounce_back ? 1 : -1);
  const size_t num_frames_total = num_frames_in + num_frames_mid + num_frames_out;
  int32_t frame_idx =
      ((normalized * num_frames_total + (ANIMATION_NORMALIZED_MAX / (2 * num_frames_total))) /
       ANIMATION_NORMALIZED_MAX);
  frame_idx = CLIP(frame_idx, 0, (int)num_frames_total - 1);


  if (normalized == ANIMATION_NORMALIZED_MAX) {
    return to;
  } else if (frame_idx < 0) {
    return from;
  } else if (frame_idx < num_frames_in) {
    return from + (frames_in ? (direction * frames_in[frame_idx]) : 0);
  } else if ((frame_idx < (num_frames_in + num_frames_mid)) && (num_frames_mid > 0)) {
    const int64_t shifted_normalized = normalized -
        (((int64_t) num_frames_in * ANIMATION_NORMALIZED_MAX) / num_frames_total);
    const int32_t mid_normalized = ((int64_t) num_frames_total * shifted_normalized) /
        num_frames_mid;
    return interpolate_int64_linear(mid_normalized,
                                    from + (direction * frames_in[num_frames_in - 1]),
                                    to + (direction_out * frames_out[0]));
  } else {
    return to + (frames_out ? (direction_out *
                               frames_out[frame_idx - (num_frames_in + num_frames_mid)]) : 0);
  }
}

int64_t interpolate_moook_in(int32_t normalized, int64_t from, int64_t to, int32_t num_frames_to) {
  return prv_interpolate_moook(normalized, from, to, s_delta_moook_in,
                               ARRAY_LENGTH(s_delta_moook_in), NULL, num_frames_to, 0, true);
}

int64_t interpolate_moook_in_only(int32_t normalized, int64_t from, int64_t to) {
  return prv_interpolate_moook(normalized, from, to, s_delta_moook_in,
                               ARRAY_LENGTH(s_delta_moook_in), NULL, 0, 0, true);
}

int64_t interpolate_moook_out(int32_t normalized, int64_t from, int64_t to,
                              int32_t num_frames_from, bool bounce_back) {
  return prv_interpolate_moook(normalized, from, to, NULL, num_frames_from, s_delta_moook_out,
                               ARRAY_LENGTH(s_delta_moook_out), 0, bounce_back);
}

int64_t interpolate_moook(int32_t normalized, int64_t from, int64_t to) {
  return prv_interpolate_moook(normalized, from, to,
                               s_delta_moook_in, ARRAY_LENGTH(s_delta_moook_in),
                               s_delta_moook_out, ARRAY_LENGTH(s_delta_moook_out), 0, true);
}

int64_t interpolate_moook_soft(int32_t normalized, int64_t from, int64_t to,
                               int32_t num_frames_mid) {
  return prv_interpolate_moook(normalized, from, to,
                               s_delta_moook_in, ARRAY_LENGTH(s_delta_moook_in),
                               s_delta_moook_out, ARRAY_LENGTH(s_delta_moook_out),
                               num_frames_mid, true);
}

int64_t interpolate_moook_custom(int32_t normalized, int64_t from, int64_t to,
                                 const MoookConfig *config) {
  PBL_ASSERTN(config);
  return prv_interpolate_moook(normalized, from, to,
                               config->frames_in, config->num_frames_in,
                               config->frames_out, config->num_frames_out,
                               config->num_frames_mid, !config->no_bounce_back);
}
