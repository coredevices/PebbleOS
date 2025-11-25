/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/heap.h"

static Heap s_worker_heap;

Heap* worker_state_get_heap(void) {
  return &s_worker_heap;
}

struct tm *worker_state_get_gmtime_tm(void) {
  static struct tm gmtime_tm = {0};
  return &gmtime_tm;
}
struct tm *worker_state_get_localtime_tm(void) {
  static struct tm localtime_tm = {0};
  return &localtime_tm;
}
char *worker_state_get_localtime_zone(void) {
  static char localtime_zone[TZ_LEN] = {0};
  return localtime_zone;
}

