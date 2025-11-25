/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "pebble_asserts.h"

#include "util/uuid.h"
#include "applib/graphics/graphics_circle_private.h"

#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_passert.h"
#include "stubs_heap.h"
#include "stubs_app_state.h"
#include "stubs_pebble_tasks.h"
#include "stubs_rand_ptr.h"

//! Welcome to the test file for all of our additional cl_assert macros.
//! You will find very basic tests, and only ones that pass (because a failure
//! would halt execution).
//!
//! You will also see that each test has a "local variable" and a "reference" section.
//! This is because it is very easy to make a macro that only works for variables and
//! and not references. This is the type of error the compiler will give if the macro
//! is built incorrectly.
//!
//! ../../tests/fw/graphics/test_graphics_circle.c:181:3: error: cannot take the address of an
//! rvalue of type 'GRect' (aka 'struct GRect')
//! cl_assert_equal_grect(GRect(-6, -6, 2, 2), grect_centered_internal(&p4, GSize(2, 2)));
//!
//! Please follow the guidelines and blend in with the other tests.

// Integer Comparisons
/////////////////////////

void test_clar__test_assert_cmp(void) {
  cl_assert_gt(2, 1);

  cl_assert_lt(1, 2);

  cl_assert_le(1, 2);
  cl_assert_le(1, 1);

  cl_assert_ge(2, 1);
  cl_assert_ge(1, 1);

  cl_assert_ne(1, 2);
}

void test_clar__test_assert_within(void) {
  cl_assert_within(0, 0, 0);
  cl_assert_within(1, 1, 1);

  cl_assert_within(1, 1, 10);
  cl_assert_within(5, 1, 10);
  cl_assert_within(10, 1, 10);

  cl_assert_within(-10, -10, 0);
  cl_assert_within(-5, -10, 0);
  cl_assert_within(0, -10, 0);

  cl_assert_within(0, -10, 0);
}

void test_clar__test_assert_near(void) {
  cl_assert_near(0, 0, 0);
  cl_assert_near(1, 1, 1);

  cl_assert_near(1, 10, 10);
  cl_assert_near(1, 5, 10);
  cl_assert_near(0, 10, 10);
}

// GRect
/////////////////////////

#define EX_GRECT GRect(5, 6, 7, 8)

static GRect prv_get_grect(void) {
  return EX_GRECT;
}

void test_clar__equal_grect(void) {
  // test with local variable
  GRect a = EX_GRECT;
  GRect b = EX_GRECT;
  cl_assert_equal_grect(a, b);

  // test with reference
  cl_assert_equal_grect(prv_get_grect(), prv_get_grect());
}

// GPoint
/////////////////////////

#define EX_GPOINT GPoint(5, 6)

static GPoint prv_get_gpoint(void) {
  return EX_GPOINT;
}

void test_clar__equal_gpoint(void) {
  // test with local variable
  GPoint a = EX_GPOINT;
  GPoint b = EX_GPOINT;
  cl_assert_equal_gpoint(a, b);

  // test with reference
  cl_assert_equal_gpoint(prv_get_gpoint(), prv_get_gpoint());
}

// GSize
/////////////////////////

#define EX_GSIZE GSize(5, 6)

static GSize prv_get_gsize(void) {
  return EX_GSIZE;
}

void test_clar__equal_gsize(void) {
  // test with local variable
  GSize a = EX_GSIZE;
  GSize b = EX_GSIZE;
  cl_assert_equal_gsize(a, b);

  // test with reference
  cl_assert_equal_gsize(prv_get_gsize(), prv_get_gsize());
}

// Uuid
/////////////////////////

#define EX_UUID ((Uuid) { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, \
                          0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff })

static Uuid prv_get_uuid(void) {
  return EX_UUID;
}

void test_clar__equal_uuid(void) {
  // test with local variable
  Uuid a = EX_UUID;
  Uuid b = EX_UUID;
  cl_assert_equal_uuid(a, b);

  // test with reference
  cl_assert_equal_uuid(prv_get_uuid(), prv_get_uuid());
}

// EllipsisDrawConfig
/////////////////////////

#define EX_QDC ((EllipsisDrawConfig) { \
                 .start_quadrant = { \
                     .angle = 1000, \
                     .quadrant = 1 \
                 }, \
                 .full_quadrants = GCornersAll, \
                 .end_quadrant = { \
                     .angle = 3000, \
                     .quadrant = 2, \
                 } \
               })

static EllipsisDrawConfig prv_get_edc(void) {
  return EX_QDC;
}

void test_clar__equal_qdc(void) {
  // test with local variable
  EllipsisDrawConfig a = EX_QDC;
  EllipsisDrawConfig b = EX_QDC;
  cl_assert_equal_edc(a, b);

  // test with reference
  cl_assert_equal_edc(prv_get_edc(), prv_get_edc());

}
