/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "process_management/pebble_process_info.h"

void test_pebble_app_info__simple(void) {
  cl_assert(version_compare((Version) { 5, 1 }, (Version) { 5, 1 }) == 0);
  cl_assert(version_compare((Version) { 5, 2 }, (Version) { 5, 1 }) > 0);
  cl_assert(version_compare((Version) { 5, 0 }, (Version) { 5, 1 }) < 0);

  cl_assert(version_compare((Version) { 4, 2 }, (Version) { 5, 1 }) < 0);
  cl_assert(version_compare((Version) { 6, 0 }, (Version) { 5, 1 }) > 0);
}

