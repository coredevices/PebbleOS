/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "watchface_metrics.h"

#include "drivers/rtc.h"
#include "os/mutex.h"
#include "services/common/regular_timer.h"
#include "services/common/system_task.h"
#include "services/normal/settings/settings_file.h"
#include "system/logging.h"
#include "util/uuid.h"

#include <string.h>

#define WATCHFACE_METRICS_FILE "wfmetrics"
#define WATCHFACE_METRICS_MAX_SIZE 256
#define WATCHFACE_METRICS_SAVE_INTERVAL_MINS 5
#define WATCHFACE_METRICS_KEY "current"

// Maximum elapsed time to accumulate per interval (guards against RTC time jumps)
// Set to slightly more than the save interval to allow for normal operation
#define WATCHFACE_METRICS_MAX_ELAPSED_SECS (WATCHFACE_METRICS_SAVE_INTERVAL_MINS * 60 + 60)

// Stored data for the current watchface
typedef struct {
  Uuid uuid;
  uint32_t total_time_secs;
} WatchfaceMetricsData;

// Runtime state (protected by s_mutex)
static struct {
  bool tracking;
  Uuid current_uuid;
  uint32_t start_ticks;
  uint32_t current_total_secs;
  uint32_t last_saved_secs;
} s_state;

static SettingsFile s_settings_file;
static bool s_initialized = false;
static RegularTimerInfo s_save_timer;
static PebbleMutex *s_mutex;

// -------------------------------------------------------------------------------------------
// Calculate elapsed seconds since start_ticks, capped to guard against RTC time jumps
static uint32_t prv_get_elapsed_secs(uint32_t start_ticks) {
  uint32_t now_ticks = rtc_get_ticks();
  uint32_t elapsed_ticks = now_ticks - start_ticks;
  uint32_t elapsed_secs = elapsed_ticks / RTC_TICKS_HZ;

  // Cap to guard against RTC time jumps (e.g., when time syncs from phone)
  if (elapsed_secs > WATCHFACE_METRICS_MAX_ELAPSED_SECS) {
    elapsed_secs = WATCHFACE_METRICS_MAX_ELAPSED_SECS;
  }
  return elapsed_secs;
}

// -------------------------------------------------------------------------------------------
static void prv_ensure_open(void) {
  if (!s_initialized) {
    status_t status = settings_file_open(&s_settings_file, WATCHFACE_METRICS_FILE,
                                         WATCHFACE_METRICS_MAX_SIZE);
    if (status != S_SUCCESS) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to open watchface metrics file: %d", (int)status);
      return;
    }
    s_initialized = true;
  }
}

// -------------------------------------------------------------------------------------------
// Load the stored watchface data. Returns true if data exists and UUID matches.
// Note: Caller must hold s_mutex
static bool prv_load_data(const Uuid *uuid, uint32_t *out_time) {
  prv_ensure_open();
  if (!s_initialized) {
    return false;
  }

  WatchfaceMetricsData data;
  status_t status = settings_file_get(&s_settings_file,
                                      WATCHFACE_METRICS_KEY, strlen(WATCHFACE_METRICS_KEY),
                                      &data, sizeof(data));
  if (status == S_SUCCESS && uuid_equal(&data.uuid, uuid)) {
    *out_time = data.total_time_secs;
    return true;
  }
  return false;
}

// -------------------------------------------------------------------------------------------
// Pending save data for async save
static struct {
  bool pending;
  Uuid uuid;
  uint32_t total_secs;
} s_pending_save;

// -------------------------------------------------------------------------------------------
// System task callback to perform the actual save (flash I/O)
// Note: This runs on the system task to avoid blocking KernelMain during app transitions.
// The flash I/O is done under s_mutex to serialize with any load operations.
static void prv_save_data_system_task_cb(void *context) {
  mutex_lock(s_mutex);

  if (!s_pending_save.pending) {
    mutex_unlock(s_mutex);
    return;
  }

  Uuid uuid = s_pending_save.uuid;
  uint32_t total_secs = s_pending_save.total_secs;
  s_pending_save.pending = false;

  prv_ensure_open();
  if (!s_initialized) {
    mutex_unlock(s_mutex);
    return;
  }

  WatchfaceMetricsData data = {
    .uuid = uuid,
    .total_time_secs = total_secs,
  };

  // Note: Flash I/O is done under mutex to serialize with load operations,
  // but this is acceptable since we're on the system task (low priority) and
  // the mutex is only used within this module.
  status_t status = settings_file_set(&s_settings_file,
                                      WATCHFACE_METRICS_KEY, strlen(WATCHFACE_METRICS_KEY),
                                      &data, sizeof(data));
  if (status != S_SUCCESS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to save watchface metrics: %d", (int)status);
  }

  mutex_unlock(s_mutex);
}

// -------------------------------------------------------------------------------------------
// Note: Caller must hold s_mutex. Schedules async save to system task.
static void prv_save_data(const Uuid *uuid, uint32_t total_secs) {
  s_pending_save.pending = true;
  s_pending_save.uuid = *uuid;
  s_pending_save.total_secs = total_secs;

  system_task_add_callback(prv_save_data_system_task_cb, NULL);
}

// -------------------------------------------------------------------------------------------
static void prv_periodic_save_callback(void *data) {
  mutex_lock(s_mutex);

  if (!s_state.tracking) {
    mutex_unlock(s_mutex);
    return;
  }

  // Accumulate elapsed time and reset start_ticks for next interval
  // This ensures time jumps only affect one interval
  uint32_t elapsed_secs = prv_get_elapsed_secs(s_state.start_ticks);
  s_state.current_total_secs += elapsed_secs;
  s_state.start_ticks = rtc_get_ticks();

  if (s_state.current_total_secs != s_state.last_saved_secs) {
    prv_save_data(&s_state.current_uuid, s_state.current_total_secs);
    s_state.last_saved_secs = s_state.current_total_secs;
    PBL_LOG(LOG_LEVEL_DEBUG, "Watchface metrics: periodic save, total: %lu secs",
            (unsigned long)s_state.current_total_secs);
  }

  mutex_unlock(s_mutex);
}

// -------------------------------------------------------------------------------------------
void watchface_metrics_init(void) {
  s_mutex = mutex_create();

  s_state.tracking = false;
  s_state.start_ticks = 0;
  s_state.current_total_secs = 0;
  s_state.last_saved_secs = 0;
  memset(&s_state.current_uuid, 0, sizeof(Uuid));

  s_save_timer = (RegularTimerInfo) {
    .cb = prv_periodic_save_callback,
  };
  regular_timer_add_multiminute_callback(&s_save_timer, WATCHFACE_METRICS_SAVE_INTERVAL_MINS);
}

// -------------------------------------------------------------------------------------------
void watchface_metrics_start(const Uuid *uuid) {
  mutex_lock(s_mutex);

  // Stop current tracking if any (inline to avoid recursive lock)
  if (s_state.tracking) {
    uint32_t elapsed_secs = prv_get_elapsed_secs(s_state.start_ticks);
    s_state.current_total_secs += elapsed_secs;
    prv_save_data(&s_state.current_uuid, s_state.current_total_secs);
    s_state.tracking = false;
  }

  s_state.current_uuid = *uuid;
  s_state.start_ticks = rtc_get_ticks();

  // Load previous time only if it's the same watchface, otherwise reset to 0
  uint32_t previous_time = 0;
  if (prv_load_data(uuid, &previous_time)) {
    s_state.current_total_secs = previous_time;
    PBL_LOG(LOG_LEVEL_DEBUG, "Watchface metrics: resuming, previous total: %lu secs",
            (unsigned long)previous_time);
  } else {
    s_state.current_total_secs = 0;
    PBL_LOG(LOG_LEVEL_DEBUG, "Watchface metrics: new watchface, starting from 0");
  }

  s_state.last_saved_secs = s_state.current_total_secs;
  s_state.tracking = true;

  mutex_unlock(s_mutex);
}

// -------------------------------------------------------------------------------------------
void watchface_metrics_stop(void) {
  mutex_lock(s_mutex);

  if (!s_state.tracking) {
    mutex_unlock(s_mutex);
    return;
  }

  uint32_t elapsed_secs = prv_get_elapsed_secs(s_state.start_ticks);
  s_state.current_total_secs += elapsed_secs;
  prv_save_data(&s_state.current_uuid, s_state.current_total_secs);
  s_state.last_saved_secs = s_state.current_total_secs;

  PBL_LOG(LOG_LEVEL_DEBUG, "Watchface metrics: stopped, session: %lu secs, total: %lu secs",
          (unsigned long)elapsed_secs, (unsigned long)s_state.current_total_secs);

  s_state.tracking = false;

  mutex_unlock(s_mutex);
}

// -------------------------------------------------------------------------------------------
uint32_t watchface_metrics_get_current_time(void) {
  mutex_lock(s_mutex);

  uint32_t result = 0;

  if (s_state.tracking) {
    uint32_t elapsed_secs = prv_get_elapsed_secs(s_state.start_ticks);
    result = s_state.current_total_secs + elapsed_secs;
  }

  mutex_unlock(s_mutex);
  return result;
}
