/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/ambient_light.h"

#include "os/mutex.h"
#include "system/passert.h"

#include <stdbool.h>
#include <stdint.h>

// Refcount framework for prime/release/suspend/resume. Drivers receive only
// the resulting (active, sampling) booleans via ambient_light_driver_set_state.

static PebbleMutex *s_mutex;
static uint16_t s_prime_refcount;
static uint16_t s_suspend_refcount;

static void prv_apply_locked(void) {
  const bool active = (s_prime_refcount > 0U);
  const bool sampling = active && (s_suspend_refcount == 0U);
  ambient_light_driver_set_state(active, sampling);
}

void ambient_light_common_init(void) {
  s_mutex = mutex_create();
}

void ambient_light_prime(void) {
  if (s_mutex == NULL) {
    return;
  }
  mutex_lock(s_mutex);
  s_prime_refcount++;
  prv_apply_locked();
  mutex_unlock(s_mutex);
}

void ambient_light_release(void) {
  if (s_mutex == NULL) {
    return;
  }
  mutex_lock(s_mutex);
  PBL_ASSERTN(s_prime_refcount > 0U);
  s_prime_refcount--;
  prv_apply_locked();
  mutex_unlock(s_mutex);
}

void ambient_light_suspend(void) {
  if (s_mutex == NULL) {
    return;
  }
  mutex_lock(s_mutex);
  s_suspend_refcount++;
  prv_apply_locked();
  mutex_unlock(s_mutex);
}

void ambient_light_resume(void) {
  if (s_mutex == NULL) {
    return;
  }
  mutex_lock(s_mutex);
  PBL_ASSERTN(s_suspend_refcount > 0U);
  s_suspend_refcount--;
  prv_apply_locked();
  mutex_unlock(s_mutex);
}
