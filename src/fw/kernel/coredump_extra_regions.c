/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/coredump_extra_regions.h"

#include "system/logging.h"

static CoredumpExtraRegion s_regions[COREDUMP_EXTRA_REGIONS_MAX];
static size_t s_count;

void coredump_extra_regions_register(const char *name, const void *addr, size_t size) {
  if (s_count >= COREDUMP_EXTRA_REGIONS_MAX) {
    PBL_LOG_ERR("coredump_extra_regions: registry full, dropping %s", name);
    return;
  }
  s_regions[s_count++] = (CoredumpExtraRegion){
      .name = name,
      .addr = (uintptr_t)addr,
      .size = size,
  };
}

const CoredumpExtraRegion *coredump_extra_regions_get(size_t *out_count) {
  *out_count = s_count;
  return s_regions;
}

// Forward declarations for per-driver registration helpers. Each driver
// provides this function (under its own platform guard); we call them all
// here under matching guards.
#if PLATFORM_OBELIX || PLATFORM_GETAFIX
void display_jdi_register_coredump_regions(void);
#endif

void coredump_extra_regions_init(void) {
#if PLATFORM_OBELIX || PLATFORM_GETAFIX
  display_jdi_register_coredump_regions();
#endif
}
