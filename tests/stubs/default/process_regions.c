/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <process_management/process_regions.h>

PBL_WEAK const struct pbl_mpu_region *process_region_app(void) {
  return nullptr;
}

PBL_WEAK const struct pbl_mpu_region *process_region_worker(void) {
  return nullptr;
}

PBL_WEAK void process_regions_thread_init(void) {
}
