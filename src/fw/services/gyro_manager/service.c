/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/gyro_manager.h"

#include "drivers/gyro.h"
#include "os/mutex.h"
#include "pbl/services/new_timer/new_timer.h"
#include "system/logging.h"
#include "system/passert.h"

static PebbleRecursiveMutex *s_gyro_manager_mutex;

static GyroDriverSample s_last_sample;
static bool s_last_sample_valid = false;

void gyro_manager_init(void) {
  s_gyro_manager_mutex = mutex_create_recursive();
}

int gyro_manager_peek(GyroDriverSample *data) {
  PBL_ASSERTN(data);

  mutex_lock_recursive(s_gyro_manager_mutex);
  int result = gyro_peek(data);
  if (result != 0 && s_last_sample_valid) {
    // Fall back to the most recent streamed sample
    *data = s_last_sample;
    result = 0;
  }
  mutex_unlock_recursive(s_gyro_manager_mutex);
  return result;
}

// Driver callbacks (see drivers/gyro.h)

void gyro_cb_new_sample(GyroDriverSample const *data) {
  // May be invoked from work offloaded through either the gyro manager or the
  // accel manager (shared-FIFO drains), so take our own lock here.
  mutex_lock_recursive(s_gyro_manager_mutex);
  s_last_sample = *data;
  s_last_sample_valid = true;
  // TODO(gyro): deliver to data service subscribers, mirroring accel_manager
  mutex_unlock_recursive(s_gyro_manager_mutex);
}

static void prv_handle_gyro_driver_work_cb(void *data) {
  mutex_lock_recursive(s_gyro_manager_mutex);
  GyroOffloadCallback cb = data;
  cb();
  mutex_unlock_recursive(s_gyro_manager_mutex);
}

void gyro_offload_work(GyroOffloadCallback cb) {
  new_timer_add_work_callback(prv_handle_gyro_driver_work_cb, cb);
}
