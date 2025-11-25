/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdio.h>

#define cl_assert_equal_grect(a, b) \
  do { \
    GRect rect_a = (a); \
    GRect rect_b = (b); \
    bool success = grect_equal(&rect_a, &rect_b); \
    if (!success) { \
      char error_msg[256] = {0}; \
      sprintf(error_msg, \
          "grect_equal(rect_a, rect_b)\n" \
          "    rect_a: {%d,%d,%d,%d}\n" \
          "    rect_b: {%d,%d,%d,%d}\n", \
          rect_a.origin.x, rect_a.origin.y, rect_a.size.w, rect_a.size.h, \
          rect_b.origin.x, rect_b.origin.y, rect_b.size.w, rect_b.size.h); \
      clar__assert(0, __FILE__, __LINE__, "Expression is not true: ", error_msg, 1); \
    } \
  } while(0)

#define cl_assert_equal_gpoint(a, b) \
  do { \
    GPoint pt_a = (a); \
    GPoint pt_b = (b); \
    bool success = gpoint_equal(&pt_a, &pt_b); \
    if (!success) { \
      char error_msg[256] = {0}; \
      sprintf(error_msg, \
          "gpoint_equal(pt_a, pt_b)\n" \
          "    pt_a: {%d,%d}\n" \
          "    pt_b: {%d,%d}\n", \
          pt_a.x, pt_a.y, \
          pt_b.x, pt_b.y); \
      clar__assert(0, __FILE__, __LINE__, "Expression is not true: ", error_msg, 1); \
    } \
  } while(0)

#define cl_assert_equal_gsize(a, b) \
  do { \
    GSize size_a = (a); \
    GSize size_b = (b); \
    bool success = gsize_equal(&size_a, &size_b); \
    if (!success) { \
      char error_msg[256] = {0}; \
      sprintf(error_msg, \
          "gsize_equal(size_a, size_b)\n" \
          "    size_a: {%d,%d}\n" \
          "    size_b: {%d,%d}\n", \
          size_a.w, size_a.h, \
          size_b.w, size_b.h); \
      clar__assert(0, __FILE__, __LINE__, "Expression is not true: ", error_msg, 1); \
    } \
  } while(0)

#define cl_assert_equal_uuid(a, b) \
  do { \
    Uuid uuid_a = (a); \
    Uuid uuid_b = (b); \
    bool success = uuid_equal(&uuid_a, &uuid_b); \
    if (!success) { \
      char error_msg[256] = {0}; \
      char uuid_a_buf[UUID_STRING_BUFFER_LENGTH] = {0}; \
      char uuid_b_buf[UUID_STRING_BUFFER_LENGTH] = {0}; \
      uuid_to_string(&uuid_a, uuid_a_buf); \
      uuid_to_string(&uuid_b, uuid_b_buf); \
      sprintf(error_msg, \
          "uuid_equal(uuid_a, uuid_b)\n" \
          "    uuid_a: %s\n" \
          "    uuid_b: %s\n", \
          uuid_a_buf, \
          uuid_b_buf); \
      clar__assert(0, __FILE__, __LINE__, "Expression is not true: ", error_msg, 1); \
    } \
  } while(0)

#define cl_assert_equal_edc(a, b) \
  do { \
    EllipsisDrawConfig edc_a = (a); \
    EllipsisDrawConfig edc_b = (b); \
    bool success = ((edc_a.start_quadrant.angle == edc_b.start_quadrant.angle) && \
                    (edc_a.start_quadrant.quadrant == edc_b.start_quadrant.quadrant) && \
                    (edc_a.full_quadrants == edc_b.full_quadrants) && \
                    (edc_a.end_quadrant.angle == edc_b.end_quadrant.angle) && \
                    (edc_a.end_quadrant.quadrant == edc_b.end_quadrant.quadrant)); \
    if (!success) { \
      char error_msg[256] = {0}; \
      sprintf(error_msg, \
          "EllipsisDrawConfig edc_a and edc_b are not equal:\n" \
          "    edc_a: start_quadrant: angle: %d quadrant: %d\n" \
          "           end_quadrant:   angle: %d quadrant: %d\n" \
          "           full_quadrants: %d\n" \
          "    edc_b: start_quadrant: angle: %d quadrant: %d\n" \
          "           end_quadrant:   angle: %d quadrant: %d\n" \
          "           full_quadrants: %d\n", \
          edc_a.start_quadrant.angle, edc_a.start_quadrant.quadrant, \
          edc_a.end_quadrant.angle, edc_a.end_quadrant.quadrant, \
          edc_a.full_quadrants, \
          edc_b.start_quadrant.angle, edc_b.start_quadrant.quadrant, \
          edc_b.end_quadrant.angle, edc_b.end_quadrant.quadrant, \
          edc_b.full_quadrants); \
      clar__assert(0, __FILE__, __LINE__, "Expression is not true: ", error_msg, 1); \
    } \
  } while (0)
