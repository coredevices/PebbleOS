/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gtransform.h"
#include "util/trig.h"

#include "util/math_fixed.h"

#include "clar.h"

#include <stdio.h>
#include <string.h>

// Helper Functions
////////////////////////////////////

// Stubs
////////////////////////////////////
#include "stubs_app_state.h"
#include "stubs_applib_resource.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_resources.h"
#include "stubs_serial.h"
#include "stubs_syscalls.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"


// Tests
////////////////////////////////////

/////////////////////////////////
/// Generic matrix tests
/////////////////////////////////
void test_graphics_gtransform_${BIT_DEPTH_NAME}__types_gtransformnumber(void) {
  GTransform t_c; // matrix to compare against
  GTransformNumber tn;
  int32_t test_num;

  tn = GTransformNumberFromNumber(1);
  test_num = (int32_t)((float)1 * (1 << FIXED_S32_16_PRECISION));
  cl_assert((memcmp(&tn, &test_num, sizeof(GTransformNumber)) == 0));

  tn = GTransformNumberFromNumber(3.5);
  test_num = (int32_t)((float)3.5 * (1 << FIXED_S32_16_PRECISION));
  cl_assert((memcmp(&tn, &test_num, sizeof(GTransformNumber)) == 0));

  tn = GTransformNumberFromNumber(-2);
  test_num = (int32_t)((float)-2 * (1 << FIXED_S32_16_PRECISION));
  cl_assert((memcmp(&tn, &test_num, sizeof(GTransformNumber)) == 0));

  tn = GTransformNumberFromNumber(-3.5);
  test_num = (int32_t)((float)-3.5 * (1 << FIXED_S32_16_PRECISION));
  cl_assert((memcmp(&tn, &test_num, sizeof(GTransformNumber)) == 0));

  t_c = GTransform(GTransformNumberFromNumber(1), GTransformNumberFromNumber(2), 
                   GTransformNumberFromNumber(3), GTransformNumberFromNumber(4), 
                   GTransformNumberFromNumber(5), GTransformNumberFromNumber(6));

  int32_t test_array[6] = {1 * (1 << FIXED_S32_16_PRECISION), 
                           2 * (1 << FIXED_S32_16_PRECISION), 
                           3 * (1 << FIXED_S32_16_PRECISION), 
                           4 * (1 << FIXED_S32_16_PRECISION), 
                           5 * (1 << FIXED_S32_16_PRECISION), 
                           6 * (1 << FIXED_S32_16_PRECISION)};

  cl_assert((memcmp(&t_c, &test_array, sizeof(GTransform)) == 0));
  cl_assert(gtransform_is_equal(&t_c, (const GTransform*)&test_array));

  t_c = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  cl_assert(gtransform_is_equal(&t_c, (const GTransform*)&test_array));

  // Test to make sure implemented rotation calculation is correct
  int32_t angle = DEG_TO_TRIGANGLE(45);
  int32_t cosine = cos_lookup(angle);
  GTransformNumber num = GTransformNumberFromNumber((float)cosine / TRIG_MAX_RATIO);
  GTransformNumber num2 = 
    (GTransformNumber) { .raw_value = (int32_t)(((int64_t)cosine * 
                                                 GTransformNumberOne.raw_value) / TRIG_MAX_RATIO) };
  cl_assert(num.raw_value == num2.raw_value);

}

void test_graphics_gtransform_${BIT_DEPTH_NAME}__types_precise(void) {
  GPointPrecise pointP = GPointPreciseFromGPoint(GPoint(2, 5));
  GPointPrecise pointP_c = GPointPrecise((2 % GPOINT_PRECISE_MAX) << GPOINT_PRECISE_PRECISION,
                                         (5 % GPOINT_PRECISE_MAX) << GPOINT_PRECISE_PRECISION);
  
  cl_assert(gpointprecise_equal(&pointP, &pointP_c));

  GVectorPrecise vectorP = GVectorPreciseFromGVector(GVector(2, 5));
  GVectorPrecise vectorP_c = GVectorPrecise((2 % GVECTOR_PRECISE_MAX) << GVECTOR_PRECISE_PRECISION,
                                            (5 % GVECTOR_PRECISE_MAX) << GVECTOR_PRECISE_PRECISION);

  cl_assert(gvectorprecise_equal(&vectorP, &vectorP_c));
}

void test_graphics_gtransform_${BIT_DEPTH_NAME}__init(void) {
  GTransform t;
  GTransform t_c; // matrix to compare against

  // Test Identity Matrix
  t = GTransformIdentity();
  t_c = GTransformFromNumbers(1, 0, 0, 1, 0, 0);

  cl_assert(gtransform_is_equal(&t, &t_c));
  cl_assert(gtransform_is_identity(&t));
  cl_assert(gtransform_is_identity(&t_c));

  // Test Scale Matrix
  t = GTransformScale(GTransformNumberFromNumber(2), GTransformNumberFromNumber(5));
  t_c = GTransformFromNumbers(2, 0, 0, 5, 0, 0);

  cl_assert(gtransform_is_equal(&t, &t_c));
  cl_assert(gtransform_is_only_scale(&t));
  cl_assert(gtransform_is_only_scale(&t_c));

  t_c = GTransformScaleFromNumber(2, 5);
  cl_assert(gtransform_is_equal(&t, &t_c));

  // Test Translation Matrix
  t = GTransformTranslation(GTransformNumberFromNumber(2), GTransformNumberFromNumber(5));
  t_c = GTransformFromNumbers(1, 0, 0, 1, 2, 5);

  cl_assert(gtransform_is_equal(&t, &t_c));
  cl_assert(gtransform_is_only_translation(&t));
  cl_assert(gtransform_is_only_translation(&t_c));

  t_c = GTransformTranslationFromNumber(2, 5);
  cl_assert(gtransform_is_equal(&t, &t_c));

  // Test Rotation Matrix
  int32_t angle = DEG_TO_TRIGANGLE(45);
  t = GTransformRotation(angle);

  int32_t cosine = cos_lookup(angle);
  int32_t sine = sin_lookup(angle);

  t_c = GTransform(GTransformNumberFromNumber((float)cosine / TRIG_MAX_RATIO),
                   GTransformNumberFromNumber(-(float)sine / TRIG_MAX_RATIO),
                   GTransformNumberFromNumber((float)sine / TRIG_MAX_RATIO),
                   GTransformNumberFromNumber((float)cosine / TRIG_MAX_RATIO),
                   GTransformNumberZero,
                   GTransformNumberZero);
  cl_assert(gtransform_is_equal(&t, &t_c));

  angle = DEG_TO_TRIGANGLE(46);
  cosine = cos_lookup(angle);
  sine = sin_lookup(angle);

  t_c = GTransform(GTransformNumberFromNumber((float)cosine / TRIG_MAX_RATIO),
                   GTransformNumberFromNumber(-(float)sine / TRIG_MAX_RATIO),
                   GTransformNumberFromNumber((float)sine / TRIG_MAX_RATIO),
                   GTransformNumberFromNumber((float)cosine / TRIG_MAX_RATIO),
                   GTransformNumberZero,
                   GTransformNumberZero);
  cl_assert(!gtransform_is_equal(&t, &t_c));

  t = GTransformRotation(0); // Should return identity if angle == 0
  cl_assert(gtransform_is_identity(&t));
}

void test_graphics_gtransform_${BIT_DEPTH_NAME}__concat(void) {
  GTransform t_new;
  GTransform t1;
  GTransform t2;
  GTransform t_c; // matrix to compare against

  // Test identity concatenation
  t1 = GTransformIdentity();
  t2 = GTransformIdentity();
  t_c = GTransformIdentity();

  gtransform_concat(&t_new, &t1, &t2);
  cl_assert(gtransform_is_equal(&t_new, &t_c));

  // Test identity concatenation with non-identity
  t1 = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  t2 = GTransformIdentity();
  t_c = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  gtransform_concat(&t_new, &t1, &t2);
  cl_assert(gtransform_is_equal(&t_new, &t_c));

  // Test pointer re-use
  gtransform_concat(&t2, &t1, &t2);
  cl_assert(gtransform_is_equal(&t2, &t_c));

  // Test non-identity concatenation with identity
  t1 = GTransformIdentity();
  t2 = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  t_c = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  gtransform_concat(&t_new, &t1, &t2);
  cl_assert(gtransform_is_equal(&t_new, &t_c));

  // Test pointer re-use
  gtransform_concat(&t1, &t1, &t2);
  cl_assert(gtransform_is_equal(&t1, &t_c));

  
  // Test concatenation of two non-identity matrices  
  t1 = GTransformFromNumbers(3, 5, 7, 11, 13, 17);
  t2 = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  t_c = GTransformFromNumbers(18, 26, 40, 58, 69, 100);
  gtransform_concat(&t_new, &t1, &t2);
  cl_assert(gtransform_is_equal(&t_new, &t_c));
}

void test_graphics_gtransform_${BIT_DEPTH_NAME}__scale(void) {
  GTransform t_new;
  GTransform t1;
  GTransform t2;
  GTransform t_c; // matrix to compare against

  // Test scaling
  t1 = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  t2 = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  t_c = GTransformFromNumbers(10, 20, 600, 800, 5, 6);

  gtransform_scale(&t_new, &t1, GTransformNumberFromNumber(10), GTransformNumberFromNumber(200));
  cl_assert(gtransform_is_equal(&t_new, &t_c));
  cl_assert(gtransform_is_equal(&t1, &t2)); // ensure t1 has not changed

  gtransform_scale_number(&t_new, &t1, 10, 200);
  cl_assert(gtransform_is_equal(&t_new, &t_c));
  cl_assert(gtransform_is_equal(&t1, &t2)); // ensure t1 has not changed

  // Test pointer re-use
  gtransform_scale(&t1, &t1, GTransformNumberFromNumber(10), GTransformNumberFromNumber(200));
  cl_assert(gtransform_is_equal(&t1, &t_c));
}

void test_graphics_gtransform_${BIT_DEPTH_NAME}__translation(void) {
  GTransform t_new;
  GTransform t1;
  GTransform t2;
  GTransform t_c; // matrix to compare against

  // Test translation
  t1 = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  t2 = GTransformFromNumbers(1, 2, 3, 4, 5, 6);
  t_c = GTransformFromNumbers(1, 2, 3, 4, 615, 826);

  gtransform_translate(&t_new, &t1, 
                       GTransformNumberFromNumber(10), GTransformNumberFromNumber(200));
  cl_assert(gtransform_is_equal(&t_new, &t_c));
  cl_assert(gtransform_is_equal(&t1, &t2)); // ensure t1 has not changed

  gtransform_translate_number(&t_new, &t1, 10, 200);
  cl_assert(gtransform_is_equal(&t_new, &t_c));
  cl_assert(gtransform_is_equal(&t1, &t2)); // ensure t1 has not changed

  // Test pointer re-use
  gtransform_translate(&t1, &t1, GTransformNumberFromNumber(10), GTransformNumberFromNumber(200));
  cl_assert(gtransform_is_equal(&t1, &t_c));
}


void test_graphics_gtransform_${BIT_DEPTH_NAME}__rotation(void) {
  GTransform t_new;
  GTransform t1;
  GTransform t2;
  GTransform t_c; // matrix to compare against

  // Test rotation
  t1 = GTransformFromNumbers(10, 10, 10, 10, 10, 10);
  t2 = GTransformFromNumbers(10, 10, 10, 10, 10, 10);
  // Initialize a, b, c, and d based on the expected result
  // a = b = 10*cos(45) - 10*sin(45)
  // c = d = 10*sin(45) + 10*cos(45)
  t_c = GTransform(GTransformNumberFromNumber(0),
                   GTransformNumberFromNumber(0), 
                   (Fixed_S32_16){ .raw_value = (int32_t)(923960) },
                   (Fixed_S32_16){ .raw_value = (int32_t)(923960) }, 
                   GTransformNumberFromNumber(10), 
                   GTransformNumberFromNumber(10));

  gtransform_rotate(&t_new, &t1, DEG_TO_TRIGANGLE(45));
  cl_assert(gtransform_is_equal(&t_new, &t_c));
  cl_assert(gtransform_is_equal(&t1, &t2)); // ensure t1 has not changed

  // Test pointer re-use
  gtransform_rotate(&t1, &t1, DEG_TO_TRIGANGLE(45));
  cl_assert(gtransform_is_equal(&t1, &t_c));
}

