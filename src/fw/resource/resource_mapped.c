/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource_mapped.h"

#include "drivers/flash.h"
#include "system/passert.h"

#if CAPABILITY_HAS_MAPPABLE_FLASH
static uint32_t s_mapped_refcount_for_task[NumPebbleTask];

void resource_mapped_use(PebbleTask task) {
  // We keep track of the refcount per task so we can cleanup all resources for a task
  s_mapped_refcount_for_task[task]++;
  flash_use();
}

void resource_mapped_release(PebbleTask task) {
  PBL_ASSERTN(s_mapped_refcount_for_task[task] != 0);
  s_mapped_refcount_for_task[task]--;
  flash_release_many(1);
}

void resource_mapped_release_all(PebbleTask task) {
  flash_release_many(s_mapped_refcount_for_task[task]);
  s_mapped_refcount_for_task[task] = 0;
}
#endif
