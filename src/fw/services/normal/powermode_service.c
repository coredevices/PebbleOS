/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/normal/powermode_service.h"

#include "drivers/cpumode.h"
#include "os/mutex.h"
#include "system/passert.h"

#include <stdint.h>

static uint32_t s_refcount;
static PebbleMutex *s_mutex;
static bool s_enabled;

void powermode_service_init(void) {
  s_refcount = 0;
  s_mutex = mutex_create();
}

void powermode_service_set_enabled(bool enabled) {
  s_enabled = enabled;
}

void powermode_service_request_hp(void) {
  if (!s_enabled) {
    return;
  }

  mutex_lock(s_mutex);

  if (s_refcount == 0) {
    cpumode_set(CPUMode_HighPerformance);
  }

  s_refcount++;

  mutex_unlock(s_mutex);
}

void powermode_service_release_hp(void) {
  if (!s_enabled) {
    return;
  }

  mutex_lock(s_mutex);

  if (s_refcount == 0) {
    mutex_unlock(s_mutex);
    return;
  }

  s_refcount--;

  if (s_refcount == 0) {
    cpumode_set(CPUMode_LowPower);
  }

  mutex_unlock(s_mutex);
}
