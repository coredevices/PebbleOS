/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <time.h>

#include <pbl/kernel/compiler.h>
#include <pbl/util/heap.h>

#include <process_state/app_state/app_state.h>

static Heap s_worker_heap;

PBL_WEAK Heap *worker_state_get_heap(void) {
  return &s_worker_heap;
}

PBL_WEAK struct pbl_tm *worker_state_get_gmtime_tm(void) {
  static struct pbl_tm gmtime_tm = {};
  return &gmtime_tm;
}

PBL_WEAK struct pbl_tm *worker_state_get_localtime_tm(void) {
  static struct pbl_tm localtime_tm = {};
  return &localtime_tm;
}

PBL_WEAK char *worker_state_get_localtime_zone(void) {
  static char localtime_zone[PBL_TZ_LEN] = {};
  return localtime_zone;
}
