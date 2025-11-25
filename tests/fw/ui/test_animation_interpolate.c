/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/animation_interpolate.h"
#include "applib/ui/animation.h"
#include "util/size.h"

#include "clar.h"

#include "stubs/stubs_logging.h"
#include "stubs/stubs_passert.h"

InterpolateInt64Function s_animation_private_current_interpolate_override;

InterpolateInt64Function animation_private_current_interpolate_override(void) {
  return s_animation_private_current_interpolate_override;
}

void test_animation_interpolate__initialize(void) {
  s_animation_private_current_interpolate_override = NULL;
}

void test_animation_interpolate__override_is_null(void) {
  cl_assert_equal_i(-10000, interpolate_int16(0, -10000, 10000));
  cl_assert_equal_i(10000, interpolate_int16(ANIMATION_NORMALIZED_MAX, -10000, 10000));
}

static AnimationProgress s_override_progress;
static int64_t s_override_from;
static int64_t s_override_to;

int64_t prv_override_capture_args(AnimationProgress p, int64_t a, int64_t b) {
  s_override_progress = p;
  s_override_from = a;
  s_override_to = b;
  return 1;
}

void test_animation_interpolate__override_gets_called(void) {
  s_animation_private_current_interpolate_override = prv_override_capture_args;
  cl_assert_equal_i(1, interpolate_int16(2, 3, 4));
  cl_assert_equal_i(2, s_override_progress);
  cl_assert_equal_i(3, s_override_from);
  cl_assert_equal_i(4, s_override_to);
}

int64_t prv_override_times_two(AnimationProgress p, int64_t a, int64_t b) {
  return interpolate_int64_linear(p, a, b) * 2;
}

void test_animation_interpolate__override_gets_clipped(void) {
  s_animation_private_current_interpolate_override = prv_override_times_two;

  cl_assert_equal_i(INT16_MIN, interpolate_int16(0, -20000, 20000));
  cl_assert_equal_i(INT16_MAX, interpolate_int16(ANIMATION_NORMALIZED_MAX, -20000, 20000));
}

void test_animation_interpolate__moook(void) {
  const int expected[] = {-20000, -19999, -19980, 20004, 20002, 20001, 20000};
  const int num_frames = ARRAY_LENGTH(expected);
  for (int i = 0; i < num_frames; i++) {
    printf("frame: %d\n", i);
    cl_assert_equal_i(interpolate_moook((i * ANIMATION_NORMALIZED_MAX) / num_frames,
                                        -20000, 20000), expected[i]);
  }
}

void test_animation_interpolate__moook_in(void) {
  const int expected[] = {-20000, -19999, -19980};
  const int num_frames = ARRAY_LENGTH(expected);
  for (int i = 0; i < num_frames; i++) {
    printf("frame: %d\n", i);
    cl_assert_equal_i(interpolate_moook_in_only((i * ANIMATION_NORMALIZED_MAX) / num_frames,
                                                -20000, 20000), expected[i]);
  }
  cl_assert_equal_i(interpolate_moook_in_only(ANIMATION_NORMALIZED_MAX,
                                              -20000, 20000), 20000);
}

void test_animation_interpolate__moook_out(void) {
  const int expected[] = {20004, 20002, 20001, 20000};
  const int num_frames = ARRAY_LENGTH(expected);
  for (int i = 0; i < num_frames; i++) {
    printf("frame: %d\n", i);
    cl_assert_equal_i(interpolate_moook_out((i * ANIMATION_NORMALIZED_MAX) / num_frames,
                                            -20000, 20000, 0, true), expected[i]);
  }
}

void test_animation_interpolate__moook_soft(void) {
  const int32_t moook_num_soft_frames = 3;
  cl_assert_equal_i(-20000, interpolate_moook_soft(0, -20000, 20000, moook_num_soft_frames));

  // mid frame is closer to end due to more end frames
  cl_assert_equal_i(6676, interpolate_moook_soft(ANIMATION_NORMALIZED_MAX / 2, -20000,
                                                 20000, moook_num_soft_frames));

  cl_assert_equal_i(20000, interpolate_moook_soft(ANIMATION_NORMALIZED_MAX, -20000,
                                                  20000, moook_num_soft_frames));
}

static const int32_t s_custom_moook_in[] = {0, 2, 8};
static const int32_t s_custom_moook_out[] = {21, 9, 3, 0};
static const MoookConfig s_custom_moook = {
  .frames_in = s_custom_moook_in,
  .num_frames_in = ARRAY_LENGTH(s_custom_moook_in),
  .frames_out = s_custom_moook_out,
  .num_frames_out = ARRAY_LENGTH(s_custom_moook_out),
  .num_frames_mid = 3,
};

void test_animation_interpolate__moook_custom(void) {
  cl_assert_equal_i(330, interpolate_moook_custom_duration(&s_custom_moook));

  cl_assert_equal_i(-20000, interpolate_moook_custom(0, -20000, 20000, &s_custom_moook));

  // mid frame is closer to end due to more end frames
  cl_assert_equal_i(6683, interpolate_moook_custom(ANIMATION_NORMALIZED_MAX / 2, -20000, 20000,
                                                   &s_custom_moook));

  cl_assert_equal_i(20000, interpolate_moook_custom(ANIMATION_NORMALIZED_MAX, -20000, 20000,
                                                    &s_custom_moook));
}
